/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * - A generic interface for accessing places.
 * - Eventually more functionality will be merged from seahorse-op.* into
 *   this class and derived classes.
 */


#ifndef __SEAHORSE_PLACE_H__
#define __SEAHORSE_PLACE_H__

#include "seahorse-types.h"

#include <gio/gio.h>

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_PLACE                (seahorse_place_get_type ())
#define SEAHORSE_PLACE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PLACE, SeahorsePlace))
#define SEAHORSE_IS_PLACE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PLACE))
#define SEAHORSE_PLACE_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_PLACE, SeahorsePlaceIface))

typedef struct _SeahorsePlace SeahorsePlace;
typedef struct _SeahorsePlaceIface SeahorsePlaceIface;

struct _SeahorsePlaceIface {
	GTypeInterface parent;

	/* virtual methods ------------------------------------------------- */

	void            (*import_async)              (SeahorsePlace *place,
	                                              GInputStream *input,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	GList *         (*import_finish)             (SeahorsePlace *place,
	                                              GAsyncResult *result,
	                                              GError **error);

	void            (*export_async)              (SeahorsePlace *place,
	                                              GList *objects,
	                                              GOutputStream *output,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	GOutputStream * (*export_finish)             (SeahorsePlace *place,
	                                              GAsyncResult *result,
	                                              GError **error);
};

GType            seahorse_place_get_type             (void) G_GNUC_CONST;

GtkActionGroup * seahorse_place_get_actions          (SeahorsePlace *self);

/* Method helper functions ------------------------------------------- */

void             seahorse_place_import_async         (SeahorsePlace *place,
                                                      GInputStream *input,
                                                      GCancellable *cancellable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data);

GList *          seahorse_place_import_finish        (SeahorsePlace *place,
                                                      GAsyncResult *result,
                                                      GError **error);

void             seahorse_place_export_async         (SeahorsePlace *place,
                                                      GList *objects,
                                                      GOutputStream *output,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

GOutputStream *  seahorse_place_export_finish        (SeahorsePlace *place,
                                                       GAsyncResult *result,
                                                       GError **error);

void             seahorse_place_export_auto_async    (GList *objects,
                                                       GOutputStream *output,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

GOutputStream *  seahorse_place_export_auto_finish   (GAsyncResult *result,
                                                       GError **error);

gboolean         seahorse_place_export_auto_wait     (GList *objects,
                                                       GOutputStream *output,
                                                       GError **error);

#endif /* __SEAHORSE_PLACE_H__ */
