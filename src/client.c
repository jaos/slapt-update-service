/*
 * Copyright (C) 2003-2026 Jason Woodward <woodwardj at jaos dot org>
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
#include "config.h"

#include "slaptService.h"
#include "common.h"
#include <libintl.h>
#include <glib.h>
#include <gtk/gtk.h>
#ifdef USE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#define _(x) gettext(x)
#define SUN_RUNNING_ICON PIXMAPS_DIR "/slapt-update-notifier-idle.png"
#define SUN_UPDATE_ICON PIXMAPS_DIR "/slapt-update-notifier-update.png"
#define SUN_TIMEOUT_RECHECK_SEC 3600 /* 1*(60*60), 1 hour */
#define SUN_TIMEOUT_DBUS_MSEC 900000 /* 1000*(15*60), 15 minutes */
#define SUN_TIMEOUT_POPUP_MSEC 10000 /* 10 seconds */
#define SUN_TRAY_PROBE_MAX_MISSES 2

struct slapt_update_notifier {
    GtkStatusIcon *tray_icon;
    GdkPixbuf *running_pixbuf;
    GdkPixbuf *updates_pixbuf;
    SlaptService *proxy;
    GtkWidget *menu;
#ifdef USE_LIBNOTIFY
    NotifyNotification *notification;
    guint notified_updates_count; /* 0 = no popup sent yet */
#endif
    guint last_checked_updates_count;
    gboolean tray_supported; /* gnome does not support a tray */
};

static struct slapt_update_notifier *sun = NULL;
static guint tray_miss_count = 0;
static gboolean tray_probed = FALSE;

#ifdef USE_LIBNOTIFY
static void close_notification(struct slapt_update_notifier *sun_instance)
{
    NotifyNotification *n = sun_instance->notification;
    gint closed_reason;

    if (n == NULL)
        return;

    /* dismissal needs no state: notified_updates_count already marks that count */
    closed_reason = notify_notification_get_closed_reason(n);
    if (closed_reason == NOTIFY_CLOSED_REASON_DISMISSED) {
        g_message("notification dismissed by the user: quiet until the update count changes");
    } else if (closed_reason == NOTIFY_CLOSED_REASON_UNSET) {
        /* may error on already closed */
        GError *error = NULL;
        if (notify_notification_close(n, &error) != TRUE) {
            g_warning("failed to close notification: %s", error->message);
            g_error_free(error);
        }
    }

    g_object_unref(G_OBJECT(n));
    sun_instance->notification = NULL;
}
#endif

static void set_hidden(void)
{
    if (sun->tray_supported) {
        gtk_status_icon_set_visible(sun->tray_icon, FALSE);
    }

#ifdef USE_LIBNOTIFY
    close_notification(sun);
#endif
}

static void run_gslapt(const char *action)
{
    gchar *argv[3];
    gchar *upgrade_arg = NULL;
    gchar *gslapt_path = g_find_program_in_path("gslapt-polkit");
    if (gslapt_path == NULL) {
        gslapt_path = g_strdup(GSLAPT_POLKIT_PATH);
    }
    argv[0] = gslapt_path;
    if (g_strcmp0(action, "upgrade") == 0) {
        upgrade_arg = g_strdup("--upgrade");
        argv[1] = upgrade_arg;
    } else {
        argv[1] = NULL;
    }
    argv[2] = NULL;

    GError *error = NULL;
    if (!g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &error)) {
        g_warning("failed to launch %s: %s", gslapt_path, error->message);
        g_error_free(error);
    }

    set_hidden();

    g_free(gslapt_path);
    g_free(upgrade_arg);
}

static void tray_clicked(GtkStatusIcon *status_icon, gpointer data)
{
    (void)status_icon;
    (void)data;
    run_gslapt("upgrade");
}

static void menuitem_hide_callback(GObject *g, gpointer data)
{
    (void)g;
    (void)data;
    set_hidden();
}

static void tray_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer data)
{
    (void)status_icon;
    (void)data;
    gtk_menu_popup(GTK_MENU(sun->menu), NULL, NULL, NULL, NULL, button, activate_time);
}

#ifdef USE_LIBNOTIFY
static void notification_action(NotifyNotification *n, char *action, gpointer user_data)
{
    (void)n;
    (void)user_data;

    g_message("notification action \"%s\" activated", action);
    run_gslapt(action);
}

static void show_notification(guint updates_count)
{
    NotifyNotification *n;

    close_notification(sun);

    if (updates_count == sun->notified_updates_count) {
        g_debug("updates unchanged (%u pending), popup already sent", updates_count);
        return;
    }

    n = notify_notification_new(
        _("New updates available"),
        _("Click on the update icon to see the available updates"),
        "info");
    notify_notification_set_timeout(n, SUN_TIMEOUT_POPUP_MSEC);
    notify_notification_add_action(
        n,
        "upgrade",
        _("Upgrade packages now"),
        NOTIFY_ACTION_CALLBACK(notification_action),
        NULL,
        NULL);

    GError *error = NULL;
    if (notify_notification_show(n, &error) != TRUE) {
        g_warning("failed to send notification: %s", error->message);
        g_error_free(error);
        g_object_unref(G_OBJECT(n));
        return;
    }

    sun->notification = n;
    sun->notified_updates_count = updates_count;
}
#endif

static void tray_destroy(struct slapt_update_notifier *sun_instance)
{
    if (sun_instance == NULL)
        return;

#ifdef USE_LIBNOTIFY
    close_notification(sun_instance);
#endif

    if (sun_instance->proxy != NULL) {
        g_object_unref(G_OBJECT(sun_instance->proxy));
        sun_instance->proxy = NULL;
    }
    if (sun_instance->updates_pixbuf != NULL) {
        g_object_unref(G_OBJECT(sun_instance->updates_pixbuf));
        sun_instance->updates_pixbuf = NULL;
    }
    if (sun_instance->running_pixbuf != NULL) {
        g_object_unref(G_OBJECT(sun_instance->running_pixbuf));
        sun_instance->running_pixbuf = NULL;
    }
    if (sun_instance->menu != NULL) {
        g_object_unref(G_OBJECT(sun_instance->menu));
        sun_instance->menu = NULL;
    }
    if (sun_instance->tray_icon != NULL) {
        g_object_unref(G_OBJECT(sun_instance->tray_icon));
        sun_instance->tray_icon = NULL;
    }
}

static void set_working(void)
{
    if (sun->tray_supported) {
        gtk_status_icon_set_from_pixbuf(sun->tray_icon, sun->running_pixbuf);
        gtk_status_icon_set_visible(sun->tray_icon, TRUE);
    }
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);
}

static void set_updates_pending(guint updates_count)
{
    if (sun->tray_supported) {
        gtk_status_icon_set_from_pixbuf(sun->tray_icon, sun->updates_pixbuf);
        gtk_status_icon_set_visible(sun->tray_icon, TRUE);
#if GTK_CHECK_VERSION(3,0,0)
        gtk_status_icon_set_tooltip_text(sun->tray_icon, _("Updates available"));
#else
        gtk_status_icon_set_tooltip(sun->tray_icon, _("Updates available"));
#endif
    }

#ifdef USE_LIBNOTIFY
    show_notification(updates_count);
#endif
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);
}

static void keep_last_updates_state(void)
{
    if (sun->last_checked_updates_count > 0)
        set_updates_pending(sun->last_checked_updates_count);
    else
        set_working();
}

static gboolean check_for_updates(gpointer userdata)
{
    (void)userdata;
    set_working();

    GError *refresh_error = NULL;
    gboolean r = slapt_service_call_refresh_cache_sync(sun->proxy, NULL, &refresh_error);
    if (!r) {
        g_warning("refresh cache failed: %s", refresh_error->message);
        g_error_free(refresh_error);
        keep_last_updates_state();
        return TRUE;
    }

    GError *updates_error = NULL;
    guint updates_count = 0;
    r = slapt_service_call_check_for_updates_sync(sun->proxy, &updates_count, NULL, &updates_error);
    if (!r) {
        g_warning("check for updates failed: %s", updates_error->message);
        g_error_free(updates_error);
        keep_last_updates_state();
        return TRUE;
    }

    /* gnome has no tray, so check for a tray being careful not to disable a working tray  */
    if (!tray_probed && gtk_status_icon_get_visible(sun->tray_icon)) {
        if (gtk_status_icon_is_embedded(sun->tray_icon)) {
            tray_probed = TRUE;
            sun->tray_supported = TRUE;
        } else if (++tray_miss_count >= SUN_TRAY_PROBE_MAX_MISSES) {
            tray_probed = TRUE;
            sun->tray_supported = FALSE;
            g_warning("no notification area available: the tray icon cannot be shown, only notifications");
        }
    }

    sun->last_checked_updates_count = updates_count;

    if (updates_count > 0) {
        set_updates_pending(updates_count);
    } else {
#ifdef USE_LIBNOTIFY
        sun->notified_updates_count = 0;
#endif
        set_hidden();
    }
    return TRUE;
}

static gboolean check_for_updates_once(gpointer userdata)
{
    (void)userdata;
    check_for_updates(NULL);
    return FALSE;
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    bindtextdomain(GETTEXT_PACKAGE, LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

#ifdef USE_LIBNOTIFY
    notify_init("slapt-update-notifier");
#endif

    sun = g_malloc0(sizeof *sun);
    sun->tray_supported = TRUE;
    sun->updates_pixbuf = gdk_pixbuf_new_from_file(SUN_UPDATE_ICON, NULL);
    sun->running_pixbuf = gdk_pixbuf_new_from_file(SUN_RUNNING_ICON, NULL);
    if (sun->updates_pixbuf == NULL || sun->running_pixbuf == NULL) {
        g_warning("failed to load tray icons from %s", PIXMAPS_DIR);
        tray_destroy(sun);
        g_free(sun);
        sun = NULL;
#ifdef USE_LIBNOTIFY
        notify_uninit();
#endif
        return -1;
    }
    sun->tray_icon = gtk_status_icon_new();

    g_signal_connect(G_OBJECT(sun->tray_icon), "activate", G_CALLBACK(tray_clicked), NULL);
    g_signal_connect(G_OBJECT(sun->tray_icon), "popup-menu", G_CALLBACK(tray_menu), NULL);
    set_working();

    sun->menu = gtk_menu_new();
    GtkWidget *hide_menuitem = gtk_menu_item_new_with_label(_("Hide"));
    gtk_menu_shell_append(GTK_MENU_SHELL(sun->menu), hide_menuitem);
    g_signal_connect_swapped(G_OBJECT(hide_menuitem), "activate", G_CALLBACK(menuitem_hide_callback), G_OBJECT(sun->tray_icon));
    gtk_widget_show_all(sun->menu);

    GError *error = NULL;
    sun->proxy = slapt_service_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
            SLAPT_SERVICE_NAMESPACE, SLAPT_SERVICE_PATH, NULL, &error);
    if (!sun->proxy) {
        g_warning("Failed to initialize DBUS proxy: %s", error->message);
        g_error_free(error);
        tray_destroy(sun);
        g_free(sun);
        sun = NULL;
#ifdef USE_LIBNOTIFY
        notify_uninit();
#endif
        return -1;
    }
    g_dbus_proxy_set_default_timeout((GDBusProxy *)sun->proxy, SUN_TIMEOUT_DBUS_MSEC);

    g_timeout_add_seconds(0, check_for_updates_once, NULL);
    g_timeout_add_seconds(SUN_TIMEOUT_RECHECK_SEC, check_for_updates, NULL);
    gtk_main();

    tray_destroy(sun);
    g_free(sun);
    sun = NULL;

#ifdef USE_LIBNOTIFY
    notify_uninit();
#endif

    return 0;
}
