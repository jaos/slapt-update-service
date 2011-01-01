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
#include <stdio.h>
#include <unistd.h>
#include "slaptService.h"
#include "server-bindings.h"
#include "common.h"
#define SLAPT_SERVICE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SLAPT_TYPE_SERVICE, SlaptServicePrivate))

struct _SlaptServicePrivate
{
  int count; /* nothing here yet */
};

G_DEFINE_TYPE (SlaptService, slapt_service, G_TYPE_OBJECT);

static gboolean slapt_service_real_check_for_updates(SlaptService *self, guint *count, GError **error)
{
  unsigned int i;
  slapt_pkg_list_t *installed_pkgs = NULL;
  slapt_pkg_list_t *avail_pkgs     = NULL;
  slapt_transaction_t *tran        = NULL;
  slapt_rc_config *rc              = NULL;

  g_assert (SLAPT_IS_SERVICE (self));

  *count = 0;

  rc = slapt_read_rc_config(SLAPT_SERVICE_DEFAULT_RC);
  if ((chdir(rc->working_dir)) == -1)
    goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;

  installed_pkgs = slapt_get_installed_pkgs();
  avail_pkgs = slapt_get_available_pkgs();
  if ( avail_pkgs == NULL || installed_pkgs == NULL )
    goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;
  if ( avail_pkgs->pkg_count == 0 )
    goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;

  tran = slapt_init_transaction();
  if (tran == NULL)
    goto SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE;

  for (i = 0; i < installed_pkgs->pkg_count; ++i) {
    slapt_pkg_info_t *installed_pkg = installed_pkgs->pkgs[i];
    slapt_pkg_info_t *update_pkg = NULL;
    slapt_pkg_info_t *newer_installed_pkg = NULL;

    if (slapt_is_excluded(rc,installed_pkg))
      continue;

    if ((newer_installed_pkg = slapt_get_newest_pkg(installed_pkgs, installed_pkg->name)) != NULL) {
      if (slapt_cmp_pkgs(installed_pkg,newer_installed_pkg) < 0)
        continue;
    }

    update_pkg = slapt_get_newest_pkg(avail_pkgs, installed_pkg->name);
    if ( update_pkg != NULL ) {

      if (slapt_cmp_pkgs(installed_pkg,update_pkg) < 0) {
        if (slapt_is_excluded(rc,update_pkg))
          continue;

        slapt_add_deps_to_trans(rc,tran,avail_pkgs, installed_pkgs,update_pkg);
        slapt_add_upgrade_to_transaction(tran,installed_pkg, update_pkg);
      } /* end if newer */

    }/* end upgrade pkg found */

  } /* end for installed_pkgs */

  /* count includes new installed as well as package upgrades */
  *count += tran->upgrade_pkgs->pkg_count + tran->install_pkgs->pkg_count;

SLAPT_SERVICE_REAL_CHECK_FOR_UPDATES_DONE:
  if (installed_pkgs != NULL)
    slapt_free_pkg_list(installed_pkgs);
  if (avail_pkgs != NULL)
    slapt_free_pkg_list(avail_pkgs);
  if (tran != NULL)
    slapt_free_transaction(tran);
  if (rc != NULL)
    slapt_free_rc_config(rc);

  return TRUE;
}

static gboolean slapt_service_real_refresh_cache(SlaptService *self)
{
  slapt_rc_config *rc = NULL;
  int r = 0;

  g_assert (SLAPT_IS_SERVICE (self));

  rc = slapt_read_rc_config(SLAPT_SERVICE_DEFAULT_RC);

  if ((chdir(rc->working_dir)) == -1)
    return FALSE;

  if (slapt_update_pkg_cache(rc) != 0)
    fprintf(stderr,"failed to update package cache... handle me\n");

  slapt_free_rc_config(rc);

  return TRUE;
}

gboolean slapt_service_check_for_updates(SlaptService *self, guint *count, GError **error)
{
  g_assert (SLAPT_IS_SERVICE (self));

  return self->check_for_updates (self, count, error);
}

gboolean slapt_service_refresh_cache(SlaptService *self)
{
  g_assert (SLAPT_IS_SERVICE (self));

  return self->refresh_cache (self);
}

static void slapt_service_class_init (SlaptServiceClass *class)
{
  GError *error = NULL;

  g_type_class_add_private (class, sizeof (SlaptServicePrivate));

  class->check_for_updates    = NULL;
  class->refresh_cache  = NULL;

  dbus_g_object_type_install_info (SLAPT_TYPE_SERVICE, &dbus_glib_slapt_service_object_info);
}

static void slapt_service_init (SlaptService *self)
{
  SlaptServiceClass *class = SLAPT_SERVICE_GET_CLASS (self);
  int request_ret;

  SlaptServicePrivate *priv;

  self->check_for_updates   = slapt_service_real_check_for_updates;
  self->refresh_cache = slapt_service_real_refresh_cache;

  self->priv = priv = SLAPT_SERVICE_GET_PRIVATE (self);
  priv->count = 0;
}

SlaptService *slapt_service_new (void)
{
  return SLAPT_SERVICE (g_object_new (SLAPT_TYPE_SERVICE, NULL));
}
