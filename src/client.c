/*
 * Copyright (C) 2003-2020 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "slaptService.h"
#include "common.h"
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#ifdef USE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#define _(x) gettext(x)
#define SUN_RUNNING_ICON PIXMAPS_DIR "/slapt-update-notifier-idle.png"
#define SUN_UPDATE_ICON PIXMAPS_DIR "/slapt-update-notifier-update.png"
#define SUN_TIMEOUT_RECHECK_SEC 360 // 1 hour
#define SUN_TIMEOUT_DBUS_MSEC 300000 /* 1000*(1*5*60), 5 minutes */
#define NOTIFICATION_DEFAULT "default"
#define NOTIFICATION_IGNORE "ignore"
#define NOTIFICATION_SHOW_UPDATES "show updates"

struct slapt_update_notifier {
    GtkStatusIcon *tray_icon;
    GdkPixbuf *running_pixbuf;
    GdkPixbuf *updates_pixbuf;
    GtkWidget *menu;
    SlaptService *proxy;
};

static struct slapt_update_notifier *sun = NULL;

static void set_hidden(void)
{
    gtk_status_icon_set_visible(sun->tray_icon, FALSE);

#ifdef USE_LIBNOTIFY
    NotifyNotification *n = g_object_get_data(G_OBJECT(sun->tray_icon), "notification");
    if (n != NULL) {
        GError *error = NULL;
        if (notify_notification_close(n, &error) != TRUE) {
            fprintf(stderr, "failed to send notification: %s\n", error->message);
            g_error_free(error);
        }
    }
#endif
}

static void run_gslapt(const char *action)
{
    gchar *argv[4];
#if defined(HAS_GSLAPTPOLKIT)
    argv[0] = "/usr/bin/gslapt-polkit";
    if (strcmp(action, "upgrade") == 0) {
        argv[1] = "--upgrade";
    } else {
        argv[1] = NULL;
    }
    argv[2] = NULL;
#elif defined(HAS_GNOMESU)
    argv[0] = "/usr/bin/gnomesu";
    argv[1] = "-c";
    if (strcmp(action, "upgrade") == 0) {
        argv[2] = "/usr/sbin/gslapt --upgrade";
    } else {
        argv[2] = "/usr/sbin/gslapt";
    }
    argv[3] = NULL;
#elif defined(HAS_GKSU)
    argv[0] = "/usr/bin/gksu";
    if (strcmp(action, "upgrade") == 0) {
        argv[1] = "/usr/sbin/gslapt --upgrade";
    } else {
        argv[1] = "/usr/sbin/gslapt";
    }
    argv[2] = NULL;
#elif defined(HAS_KDESU)
    argv[0] = "/usr/bin/kdesu";
    if (strcmp(action, "upgrade") == 0) {
        argv[1] = "/usr/sbin/gslapt --upgrade";
    } else {
        argv[1] = "/usr/sbin/gslapt";
    }
    argv[2] = NULL;
#else
#error unable to create command to run gslapt
#endif

    g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, NULL);
    set_hidden();
}

void tray_clicked(GtkStatusIcon *status_icon, gpointer data)
{
    run_gslapt("upgrade");
}

void tray_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer data)
{
    gtk_menu_popup(
        GTK_MENU(sun->menu),
        NULL, NULL, NULL, NULL,
        button, activate_time);
}

#ifdef USE_LIBNOTIFY
static void notify_callback(NotifyNotification *handle, const char *action, void *user_data)
{
    if (strcmp(action, NOTIFICATION_SHOW_UPDATES) == 0) {
#if GTK_CHECK_VERSION(3,0,0)
        gtk_widget_hide(GTK_WIDGET(sun->tray_icon));
#else
        gtk_widget_hide_all(GTK_WIDGET(sun->tray_icon));
#endif
        run_gslapt("upgrade");
    }

    if (handle != NULL) {
        GError *error = NULL;
        if (notify_notification_close(handle, &error) != TRUE) {
            fprintf(stderr, "failed to send notification: %s\n", error->message);
            g_error_free(error);
        }
    }
}

gboolean show_notification(gpointer data)
{
    NotifyNotification *n;

    n = notify_notification_new(
        _("New updates available"),
        _("Click on the update icon to see the available updates"),
        "info");

    GError *error = NULL;
    if (notify_notification_show(n, &error) != TRUE) {
        fprintf(stderr, "failed to send notification: %s\n", error->message);
        g_error_free(error);
    }

    g_object_unref(G_OBJECT(n));

    return FALSE;
}
#endif

void tray_destroy(struct slapt_update_notifier *sun)
{
    if (sun->updates_pixbuf)
        g_object_unref(G_OBJECT(sun->updates_pixbuf));
    if (sun->running_pixbuf)
        g_object_unref(G_OBJECT(sun->running_pixbuf));

    g_object_unref(G_OBJECT(sun->tray_icon));
}

static void set_working(void)
{
    gtk_status_icon_set_from_pixbuf(sun->tray_icon, sun->running_pixbuf);
    gtk_status_icon_set_visible(sun->tray_icon, TRUE);
    while (gtk_events_pending())
        gtk_main_iteration();
}

static void set_updates_pending(void)
{
    gtk_status_icon_set_from_pixbuf(sun->tray_icon, sun->updates_pixbuf);
    gtk_status_icon_set_visible(sun->tray_icon, TRUE);
#if GTK_CHECK_VERSION(3,0,0)
    gtk_status_icon_set_tooltip_text(sun->tray_icon, _("Updates available"));
#else
    gtk_status_icon_set_tooltip(sun->tray_icon, _("Updates available"));
#endif

#ifdef USE_LIBNOTIFY
    show_notification(NULL);
#endif
    while (gtk_events_pending())
        gtk_main_iteration();
}

static gboolean check_for_updates(gpointer userdata)
{
    /* set working icon */
    set_working();

    GError *refresh_error = NULL;
    gboolean r = slapt_service_call_refresh_cache_sync(sun->proxy, NULL, &refresh_error);
    if (!r) {
        g_warning("refresh cache failed: %s", refresh_error->message);
        g_error_free(refresh_error);
        set_hidden();
        return TRUE;
    }

    GError *updates_error = NULL;
    guint count = 0;
    r = slapt_service_call_check_for_updates_sync(sun->proxy, &count, NULL, &updates_error);
    if (!r) {
        g_warning("check for updates failed: %s", updates_error->message);
        g_error_free(updates_error);
        set_hidden();
        return TRUE;
    }
    if (count > 0) {
        set_updates_pending();
    } else {
        set_hidden();
    }
    return TRUE;
}

static gboolean check_for_updates_once(gpointer userdata)
{
    check_for_updates(NULL);
    return FALSE;
}

/*
void menuitem_check_callback(GObject *g, void *dat)
{
    check_for_updates_once(NULL);
}
*/

void menuitem_hide_callback(GObject *g, void *data)
{
    set_hidden();
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
#ifdef USE_LIBNOTIFY
    notify_init("slapt-update-notifier");
#endif

    sun = malloc(sizeof *sun);
    sun->updates_pixbuf = gdk_pixbuf_new_from_file(SUN_UPDATE_ICON, NULL);
    sun->running_pixbuf = gdk_pixbuf_new_from_file(SUN_RUNNING_ICON, NULL);
    sun->tray_icon = gtk_status_icon_new();

    g_signal_connect(G_OBJECT(sun->tray_icon), "activate", G_CALLBACK(tray_clicked), NULL);
    g_signal_connect(G_OBJECT(sun->tray_icon), "popup-menu", G_CALLBACK(tray_menu), NULL);
    set_working();

    sun->menu = gtk_menu_new();
    GtkWidget *hide_menuitem = gtk_menu_item_new_with_label(_("Hide"));
    gtk_menu_shell_append(GTK_MENU_SHELL(sun->menu), hide_menuitem);
    g_signal_connect(G_OBJECT(hide_menuitem), "activate", G_CALLBACK(menuitem_hide_callback), NULL);
    gtk_widget_show_all(sun->menu);

    GError *error = NULL;
    sun->proxy = slapt_service_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
            SLAPT_SERVICE_NAMESPACE, SLAPT_SERVICE_PATH, NULL, &error);
    if (!sun->proxy) {
        g_warning("Failed to initialize DBUS proxy: %s", error->message);
        g_error_free(error);
        tray_destroy(sun);
        free(sun);
#ifdef USE_LIBNOTIFY
        notify_uninit();
#endif
        return -1;
    }
    g_dbus_proxy_set_default_timeout(sun->proxy, SUN_TIMEOUT_DBUS_MSEC);

    g_timeout_add_seconds(0, (GSourceFunc)check_for_updates_once, NULL);
    g_timeout_add_seconds(SUN_TIMEOUT_RECHECK_SEC, (GSourceFunc)check_for_updates, NULL);
    gtk_main();

    tray_destroy(sun);
    free(sun);

#ifdef USE_LIBNOTIFY
    notify_uninit();
#endif

    return 0;
}
