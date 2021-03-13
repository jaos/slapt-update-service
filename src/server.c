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
#include "slaptService.h"
#include "common.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <syslog.h>
#include <slapt.h>

static gboolean handle_refresh_cache(SlaptService *srv, GDBusMethodInvocation *meth, gpointer user_data)
{
    slapt_config_t *rc = slapt_config_t_read(SLAPT_SERVICE_DEFAULT_RC);
    slapt_working_dir_init(rc);
    if ((chdir(rc->working_dir)) == -1) {
        slapt_config_t_free(rc);
        return TRUE;
    }

    if (slapt_update_pkg_cache(rc) != 0) {
        slapt_config_t_free(rc);
        return TRUE;
    }

    slapt_config_t_free(rc);
    slapt_service_complete_refresh_cache(srv, meth);
    return TRUE;
}

static gboolean handle_check_for_updates(SlaptService *srv, GDBusMethodInvocation *meth, gpointer user_data)
{
    guint count = 0;
    slapt_config_t *rc = slapt_config_t_read(SLAPT_SERVICE_DEFAULT_RC);
    slapt_working_dir_init(rc);
    if ((chdir(rc->working_dir)) == -1) {
        slapt_config_t_free(rc);
        return TRUE;
    }

    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();
    slapt_vector_t *avail_pkgs = slapt_get_available_pkgs();
    if (avail_pkgs == NULL || installed_pkgs == NULL)
        goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;
    if (avail_pkgs->size == 0)
        goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;

    slapt_transaction_t *tran = slapt_transaction_t_init();
    if (tran == NULL)
        goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;

    slapt_vector_t_foreach(slapt_pkg_t *, installed_pkg, installed_pkgs) {
        slapt_pkg_t *update_pkg = NULL;
        slapt_pkg_t *newer_installed_pkg = NULL;
        if (slapt_is_excluded(rc, installed_pkg))
            continue;
        if ((newer_installed_pkg = slapt_get_newest_pkg(installed_pkgs, installed_pkg->name)) != NULL) {
            if (slapt_pkg_t_cmp(installed_pkg, newer_installed_pkg) < 0)
                continue;
        }
        update_pkg = slapt_get_newest_pkg(avail_pkgs, installed_pkg->name);
        if (update_pkg != NULL) {
            if (slapt_pkg_t_cmp(installed_pkg, update_pkg) < 0) {
                if (slapt_is_excluded(rc, update_pkg))
                    continue;
                if (slapt_transaction_t_add_dependencies(rc, tran, avail_pkgs, installed_pkgs, update_pkg) == 0)
                    slapt_transaction_t_add_upgrade(tran, installed_pkg, update_pkg);
            } /* end if newer */
        } /* end upgrade pkg found */
    } /* end for installed_pkgs */

    /* count includes new installed as well as package upgrades */
    count += tran->upgrade_pkgs->size + tran->install_pkgs->size;

SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE:
    if (installed_pkgs != NULL)
        slapt_vector_t_free(installed_pkgs);
    if (avail_pkgs != NULL)
        slapt_vector_t_free(avail_pkgs);
    if (tran != NULL)
        slapt_transaction_t_free(tran);
    if (rc != NULL)
        slapt_config_t_free(rc);

    slapt_service_complete_check_for_updates(srv, meth, count);
    return TRUE;
}

static void on_name_lost(GDBusConnection *c, const gchar *name, gpointer user_data)
{
    g_warning("server name lost");
    exit(1);
}

static void on_name_acquired(GDBusConnection *c, const gchar *name, gpointer user_data)
{
    SlaptService *srv = slapt_service_skeleton_new();
    g_signal_connect(srv, "handle-check-for-updates", G_CALLBACK(handle_check_for_updates), NULL);
    g_signal_connect(srv, "handle-refresh-cache", G_CALLBACK(handle_refresh_cache), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(srv), c, SLAPT_SERVICE_PATH, NULL);
}

int main(void)
{

#if !GLIB_CHECK_VERSION(2,35,0)
    g_type_init ();
#endif
#ifdef SLAPT_HAS_GPGME
    gpgme_check_version(NULL);
#ifdef ENABLE_NLS
    gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
#endif
#endif
    curl_global_init(CURL_GLOBAL_ALL);

    /* this is to silence the bad use of stdout in libslapt */
    FILE *newstdout = freopen("/dev/null", "w", stdout);
    if (newstdout == NULL) {
        fprintf(stderr, "failed to close stdout\n");
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    guint owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM, SLAPT_SERVICE_NAMESPACE, G_BUS_NAME_OWNER_FLAGS_NONE, NULL, on_name_acquired, on_name_lost, NULL, NULL);
    g_main_loop_run(loop);
    g_bus_unown_name(owner_id);
    g_main_loop_unref (loop);

    return 0;
}
