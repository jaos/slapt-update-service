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
#ifndef __SLAPT_SERVICE_H__
#define __SLAPT_SERVICE_H__

#include <glib-object.h>
#include <dbus/dbus-glib-bindings.h>
#include <slapt.h>

#define SLAPT_TYPE_SERVICE                  (slapt_service_get_type ())
#define SLAPT_SERVICE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SLAPT_TYPE_SERVICE, SlaptService))
#define SLAPT_IS_SERVICE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SLAPT_TYPE_SERVICE))
#define SLAPT_SERVICE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SLAPT_TYPE_SERVICE, SlaptServiceClass))
#define SLAPT_IS_SERVICE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SLAPT_TYPE_SERVICE))
#define SLAPT_SERVICE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SLAPT_TYPE_SERVICE, SlaptServiceClass))

typedef struct _SlaptService        SlaptService;
typedef struct _SlaptServiceClass   SlaptServiceClass;
typedef struct _SlaptServicePrivate SlaptServicePrivate;

struct _SlaptService
{
  GObject parent_instance;

  SlaptServicePrivate *priv;

  gboolean (*check_for_updates) (SlaptService *self, guint *count, GError **error);
  gboolean (*refresh_cache) (SlaptService *self);
};

struct _SlaptServiceClass
{
  GObjectClass parent_class;

  DBusGConnection *connection;

  gboolean (*check_for_updates) (SlaptService *self, guint *count, GError **error);
  gboolean (*refresh_cache) (SlaptService *self);
};

/* used by SLAPT_TYPE_SERVICE */
GType slapt_service_get_type (void);

SlaptService *slapt_service_new (void);
/*
 * Method definitions.
 */
gboolean slapt_service_check_for_updates(SlaptService *self, guint *count, GError **error);
gboolean slapt_service_refresh_cache(SlaptService *self);

#endif /* __SLAPT_SERVICE_H__ */

