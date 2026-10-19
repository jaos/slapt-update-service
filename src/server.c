/*
 * Copyright (C) 2003-2026 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>
 */
#include "config.h"
#include "slaptService.h"
#include "common.h"
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <slapt.h>

static GMainLoop *main_loop = NULL;
static SlaptService *service_instance = NULL;
static gboolean name_held = FALSE;
static time_t last_refresh = 0;

static void restore_cwd(gint saved_fd)
{
    if (saved_fd != -1) {
        if (fchdir(saved_fd) == -1) {
            g_warning("failed to restore working directory: %s", g_strerror(errno));
        }
        close(saved_fd);
    }
}

static gboolean handle_refresh_cache(SlaptService *srv, GDBusMethodInvocation *meth, gpointer user_data)
{
    (void)user_data;

    time_t now = time(NULL);
    if (last_refresh != 0 && now - last_refresh < SLAPT_REFRESH_CACHE_MIN_INTERVAL_SEC) {
        slapt_service_complete_refresh_cache(srv, meth);
        return TRUE;
    }

    slapt_config_t *rc = slapt_config_t_read(SLAPT_SERVICE_DEFAULT_RC);
    if (rc == NULL) {
        g_dbus_method_invocation_return_error(meth,
            G_IO_ERROR, G_IO_ERROR_FAILED,
            "failed to read config file");
        return TRUE;
    }

    slapt_working_dir_init(rc);
    gint saved_fd = open(".", O_RDONLY);
    if ((chdir(rc->working_dir)) == -1) {
        restore_cwd(saved_fd);
        slapt_config_t_free(rc);
        g_dbus_method_invocation_return_error(meth,
            G_IO_ERROR, G_IO_ERROR_FAILED,
            "failed to change to working directory");
        return TRUE;
    }

    if (slapt_update_pkg_cache(rc) != 0) {
        restore_cwd(saved_fd);
        slapt_config_t_free(rc);
        g_dbus_method_invocation_return_error(meth,
            G_IO_ERROR, G_IO_ERROR_FAILED,
            "package cache refresh failed");
        return TRUE;
    }

    restore_cwd(saved_fd);
    slapt_config_t_free(rc);
    last_refresh = now;
    slapt_service_complete_refresh_cache(srv, meth);
    return TRUE;
}

static gboolean handle_check_for_updates(SlaptService *srv, GDBusMethodInvocation *meth, gpointer user_data)
{
    (void)user_data;
    guint count = 0;
    slapt_config_t *rc = slapt_config_t_read(SLAPT_SERVICE_DEFAULT_RC);
    if (rc == NULL) {
        g_dbus_method_invocation_return_error(meth,
            G_IO_ERROR, G_IO_ERROR_FAILED,
            "failed to read config file");
        return TRUE;
    }

    slapt_working_dir_init(rc);
    gint saved_fd = open(".", O_RDONLY);
    if ((chdir(rc->working_dir)) == -1) {
        restore_cwd(saved_fd);
        slapt_config_t_free(rc);
        g_dbus_method_invocation_return_error(meth,
            G_IO_ERROR, G_IO_ERROR_FAILED,
            "failed to change to working directory");
        return TRUE;
    }

    slapt_transaction_t *trxn = slapt_transaction_t_init();
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();
    slapt_vector_t *avail_pkgs = slapt_get_available_pkgs(rc);

    if (avail_pkgs == NULL || installed_pkgs == NULL || trxn == NULL || avail_pkgs->size == 0) {
        goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;
    }

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
                if (slapt_transaction_t_add_dependencies(rc, trxn, avail_pkgs, installed_pkgs, update_pkg) == 0)
                    slapt_transaction_t_add_upgrade(trxn, installed_pkg, update_pkg);
            } /* end if newer */
        } /* end upgrade pkg found */
    } /* end for installed_pkgs */

    /* count includes new installed as well as package upgrades */
    count += trxn->upgrade_pkgs->size + trxn->install_pkgs->size;

SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE:
    restore_cwd(saved_fd);
    if (installed_pkgs != NULL)
        slapt_vector_t_free(installed_pkgs);
    if (avail_pkgs != NULL)
        slapt_vector_t_free(avail_pkgs);
    if (trxn != NULL)
        slapt_transaction_t_free(trxn);
    if (rc != NULL)
        slapt_config_t_free(rc);

    slapt_service_complete_check_for_updates(srv, meth, count);
    return TRUE;
}

static void on_name_lost(GDBusConnection *c, const gchar *name, gpointer user_data)
{
    (void)c;
    (void)name;
    (void)user_data;
    g_warning("server name lost");
    g_main_loop_quit(main_loop);
}

static void on_name_acquired(GDBusConnection *c, const gchar *name, gpointer user_data)
{
    (void)name;
    (void)user_data;
    name_held = TRUE;
    service_instance = slapt_service_skeleton_new();
    g_signal_connect(service_instance, "handle-check-for-updates", G_CALLBACK(handle_check_for_updates), NULL);
    g_signal_connect(service_instance, "handle-refresh-cache", G_CALLBACK(handle_refresh_cache), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(service_instance), c, SLAPT_SERVICE_PATH, NULL);
}

int main(void)
{
#ifdef SLAPT_HAS_GPGME
    gpgme_check_version(NULL);
#endif
    curl_global_init(CURL_GLOBAL_ALL);

    /* this is to silence the bad use of stdout in libslapt */
    FILE *newstdout = freopen("/dev/null", "w", stdout);
    if (newstdout == NULL) {
        fprintf(stderr, "failed to close stdout\n");
    }

    main_loop = g_main_loop_new(NULL, FALSE);
    guint owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM, SLAPT_SERVICE_NAMESPACE,
        G_BUS_NAME_OWNER_FLAGS_NONE, NULL, on_name_acquired, on_name_lost, NULL, NULL);
    g_main_loop_run(main_loop);
    g_bus_unown_name(owner_id);
    if (service_instance != NULL)
        g_object_unref(G_OBJECT(service_instance));
    g_main_loop_unref(main_loop);

    return name_held ? 0 : -1;
}
