/*
 * Copyright (C) 2003-2011 Jason Woodward <woodwardj at jaos dot org>
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
#  include <config.h>
#endif

#include "client-bindings.h"
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
#define SUN_TIMEOUT_RECHECK 14400000 /* 1000*(4*60*60), 4 hours */
#define NOTIFICATION_DEFAULT "default"
#define NOTIFICATION_IGNORE "ignore"
#define NOTIFICATION_SHOW_UPDATES "show updates"

struct slapt_update_notifier {
  GtkStatusIcon *tray_icon;
  GdkPixbuf *running_pixbuf;
  GdkPixbuf *updates_pixbuf;
  GtkWidget *menu;
  DBusGConnection *bus;
  DBusGProxy *proxy;
};

static struct slapt_update_notifier *sun = NULL;
static void hide_sun (void);

static void run_gslapt (const char *action)
{
  gchar *argv[4];
#if defined(HAS_GNOMESU)
  argv[0] = "/usr/bin/gnomesu";
  argv[1] = "-c";
  if ( strcmp(action,"upgrade") == 0 ) {
    argv[2] = "/usr/sbin/gslapt --upgrade";
  } else {
    argv[2] = "/usr/sbin/gslapt";
  }
  argv[3] = NULL;
#elif defined(HAS_GKSU)
  argv[0] = "/usr/bin/gksu";
  if ( strcmp(action,"upgrade") == 0 ) {
    argv[1] = "/usr/sbin/gslapt --upgrade";
  } else {
    argv[1] = "/usr/sbin/gslapt";
  }
  argv[2] = NULL;
#elif defined(HAS_KDESU)
  argv[0] = "/usr/bin/kdesu";
  if ( strcmp(action,"upgrade") == 0 ) {
    argv[1] = "/usr/sbin/gslapt --upgrade";
  } else {
    argv[1] = "/usr/sbin/gslapt";
  }
  argv[2] = NULL;
#else
  #error unable to create command to run gslapt
#endif

  g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL);
  hide_sun();
}

void tray_clicked (GtkStatusIcon *status_icon, gpointer data)
{
  run_gslapt("upgrade");
}

void tray_menu (GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer data)
{
  gtk_menu_popup(
    GTK_MENU(sun->menu),
    NULL,NULL,NULL,NULL,
    button,activate_time
  );
}

#ifdef USE_LIBNOTIFY
static void notify_callback(NotifyNotification *handle, const char *action, void *user_data)
{
  GMainLoop *loop = (GMainLoop*)user_data;

  if ( strcmp(action,NOTIFICATION_SHOW_UPDATES) == 0 ) {
    gtk_widget_hide_all(GTK_WIDGET(sun->tray_icon));
    run_gslapt("upgrade");
  }

  if (handle != NULL) {
    GError *error = NULL;
    if (notify_notification_close(handle, &error) != TRUE) {
      fprintf(stderr, "failed to send notification: %s\n", error->message);
      g_error_free (error);
    }
  }

  if (loop != NULL)
    g_main_loop_quit(loop);

}

gboolean show_notification (gpointer data)
{
  NotifyNotification *n;

  n = notify_notification_new (
    _("New updates available"),
    _("Click on the update icon to see the available updates"),
    "info", NULL
  );

  GError *error = NULL;
  if (notify_notification_show(n, &error) != TRUE)
  {
    fprintf(stderr, "failed to send notification: %s\n", error->message);
    g_error_free (error);
  }

  g_object_unref(G_OBJECT(n));

  return FALSE;
}
#endif

void tray_destroy (struct slapt_update_notifier *sun)
{

  if (sun->updates_pixbuf)
    g_object_unref(G_OBJECT(sun->updates_pixbuf));
  if (sun->running_pixbuf)
    g_object_unref(G_OBJECT(sun->running_pixbuf));

  gtk_widget_destroy(GTK_WIDGET(sun->tray_icon));
}

static void hide_sun (void)
{
#ifdef USE_LIBNOTIFY
  NotifyNotification *n = g_object_get_data(G_OBJECT(sun->tray_icon),"notification");
#endif
  GMainLoop *loop = g_object_get_data(G_OBJECT(sun->tray_icon), "notification_loop");

  gtk_status_icon_set_visible(sun->tray_icon, FALSE);

#ifdef USE_LIBNOTIFY
  if ( n != NULL ) {
    GError *error = NULL;
    if (notify_notification_close(n, &error) != TRUE) {
      fprintf(stderr, "failed to send notification: %s\n", error->message);
      g_error_free (error);
    }
  }
#endif
  
  if ( loop != NULL )
    g_main_loop_quit(loop);

}

void menuitem_hide_callback (GObject *g, void *data)
{
  hide_sun();
}

static void check_for_updates_callback (DBusGProxy *proxy, guint OUT_count, GError *error, gpointer userdata)
{

  if (OUT_count == 0)
    return;

  if (error != NULL) {
    g_warning ("check for updates failed: %s",
         error->message);
    g_error_free (error);
  }

  gtk_status_icon_set_visible(sun->tray_icon, TRUE);
#ifdef USE_LIBNOTIFY
  show_notification(NULL);
#endif
}

static void refresh_cache_callback (DBusGProxy *proxy, GError *error, gpointer userdata)
{

  if (error != NULL) {
    g_warning ("refresh cache failed: %s",
         error->message);
    g_error_free (error);
  }

  /* unset working icon */
  gtk_status_icon_set_visible(sun->tray_icon, FALSE);
  gtk_status_icon_set_from_pixbuf (sun->tray_icon, sun->updates_pixbuf);
}

static gboolean check_for_updates (gpointer userdata)
{
  DBusGProxy *proxy = (DBusGProxy *)((struct slapt_update_notifier *)userdata)->proxy;

  /* set working icon */
  gtk_status_icon_set_from_pixbuf (sun->tray_icon, sun->running_pixbuf);
  gtk_status_icon_set_visible(sun->tray_icon, TRUE);

  org_jaos_SlaptService_refresh_cache_async (proxy, refresh_cache_callback, NULL);
  org_jaos_SlaptService_check_for_updates_async (proxy, check_for_updates_callback, NULL);
}

int main (int argc, char *argv[])
{
  GError *error = NULL;
  guint count;
  GtkWidget *menuitem = NULL;

  gtk_init (&argc, &argv);
#ifdef USE_LIBNOTIFY
  notify_init("slapt-update-notifier");
#endif

  sun = malloc(sizeof *sun);
  sun->tray_icon        = gtk_status_icon_new ();
  sun->updates_pixbuf   = gdk_pixbuf_new_from_file(SUN_UPDATE_ICON, NULL);
  sun->running_pixbuf   = gdk_pixbuf_new_from_file(SUN_RUNNING_ICON, NULL);
  gtk_status_icon_set_from_pixbuf (sun->tray_icon, sun->updates_pixbuf);
  gtk_status_icon_set_tooltip (sun->tray_icon, _("Updates available"));
  gtk_status_icon_set_visible(sun->tray_icon, FALSE);

  g_signal_connect(G_OBJECT(sun->tray_icon), "activate", G_CALLBACK(tray_clicked), &sun);
  g_signal_connect(G_OBJECT(sun->tray_icon), "popup-menu", G_CALLBACK(tray_menu), &sun);

  sun->menu = gtk_menu_new();
  menuitem = gtk_menu_item_new_with_label(_("Hide"));
  gtk_menu_shell_append(GTK_MENU_SHELL(sun->menu),menuitem);
  g_signal_connect(G_OBJECT(menuitem),"activate",G_CALLBACK(menuitem_hide_callback),sun);
  gtk_widget_show_all(sun->menu);

  sun->bus  = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
  if (sun->bus == NULL) {
    g_warning ("Failed to make connection to system bus: %s",
         error->message);
    g_error_free (error);
    exit(1);
  }

  sun->proxy = dbus_g_proxy_new_for_name (sun->bus, "org.jaos.SlaptService","/org/jaos/SlaptService",
             "org.jaos.SlaptService");

  check_for_updates ((gpointer)sun);

  g_timeout_add(SUN_TIMEOUT_RECHECK,(GSourceFunc)check_for_updates,sun);

  gtk_main();

  tray_destroy(sun);
  g_object_unref (sun->proxy);
  free(sun);

#ifdef USE_LIBNOTIFY
  notify_uninit();
#endif

  return 0;
}
