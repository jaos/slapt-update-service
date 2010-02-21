/*
 * Copyright (C) 2003-2010 Jason Woodward <woodwardj at jaos dot org>
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
#include "slaptService.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <syslog.h>

static GMainLoop *loop;

int main (void)
{
  DBusGConnection *bus;
  DBusGProxy *proxy;
  GError *error = NULL;
  guint32 ret;
  SlaptService *slapt_service = NULL;
  int saved_stdout = dup(1);
  FILE *newstdout = NULL;

  g_type_init();

  bus  = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
  if (bus == NULL) {
    syslog (LOG_DAEMON | LOG_INFO,
        "Failed to make connection to system bus: %s",
        error->message);
    g_error_free (error);
    exit(1);
  }

  proxy = dbus_g_proxy_new_for_name (bus, DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
  if (!org_freedesktop_DBus_request_name (proxy, SLAPT_SERVICE_NAMESPACE, 0, &ret, &error)) {
    syslog (LOG_DAEMON | LOG_INFO,
        "There was an error requesting the name: %s",
        error->message);
    g_error_free (error);
    exit(1);
  }

  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    exit(1);

  slapt_service = slapt_service_new ();

  dbus_g_connection_register_g_object (bus,
               SLAPT_SERVICE_PATH,
               G_OBJECT (slapt_service));


  /* this is to silence the bad use of stdout in libslapt */
  newstdout = freopen("/dev/null", "w", stdout);
  if (newstdout == NULL) {
    fprintf(stderr,"failed to close stdout\n");
  }


  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  return 0;
}
