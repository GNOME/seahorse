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
 * SeahorseSource: Base class for other sources. 
 * 
 * - A generic interface for accessing sources.
 * - Eventually more functionality will be merged from seahorse-op.* into 
 *   this class and derived classes. 
 * - Each SeahorseObject has a weak pointer to the SeahorseSource that 
 *   created it.
 * 
 * Properties base classes must implement:
 *  ktype: (GQuark) The ktype (ie: SEAHORSE_PGP) of objects originating from this 
 *         object source.
 *  location: (SeahorseLocation) The location of objects that come from this 
 *         source. (ie: SEAHORSE_LOCATION_LOCAL, SEAHORSE_LOCATION_REMOTE)
 *  uri: (gchar*) Only for remote object sources. The full URI of the keyserver 
 *         being used.
 */


#ifndef __SEAHORSE_SOURCE_H__
#define __SEAHORSE_SOURCE_H__

#include "seahorse-types.h"

#include <gio/gio.h>
#include <glib-object.h>

#define SEAHORSE_TYPE_SOURCE                (seahorse_source_get_type ())
#define SEAHORSE_SOURCE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SOURCE, SeahorseSource))
#define SEAHORSE_IS_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SOURCE))
#define SEAHORSE_SOURCE_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_SOURCE, SeahorseSourceIface))

struct _SeahorseObject;

typedef struct _SeahorseSource SeahorseSource;
typedef struct _SeahorseSourceIface SeahorseSourceIface;

struct _SeahorseSourceIface {
	GTypeInterface parent;

	/* virtual methods ------------------------------------------------- */

	void            (*load_async)                (SeahorseSource *source,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	gboolean        (*load_finish)               (SeahorseSource *source,
	                                              GAsyncResult *result,
	                                              GError **error);

	void            (*search_async)              (SeahorseSource *source,
	                                              const gchar *match,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	GList *         (*search_finish)             (SeahorseSource *source,
	                                              GAsyncResult *result,
	                                              GError **error);

	void            (*import_async)              (SeahorseSource *source,
	                                              GInputStream *input,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	GList *         (*import_finish)             (SeahorseSource *source,
	                                              GAsyncResult *result,
	                                              GError **error);

	void            (*export_async)              (SeahorseSource *source,
	                                              GList *objects,
	                                              GOutputStream *output,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	GOutputStream * (*export_finish)             (SeahorseSource *source,
	                                              GAsyncResult *result,
	                                              GError **error);

	void            (*export_raw_async)          (SeahorseSource *source,
	                                              GList *ids,
	                                              GOutputStream *output,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	GOutputStream * (*export_raw_finish)         (SeahorseSource *source,
	                                              GAsyncResult *result,
	                                              GError **error);
};

GType            seahorse_source_get_type             (void) G_GNUC_CONST;

/* Method helper functions ------------------------------------------- */

void             seahorse_source_load_async           (SeahorseSource *source,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

gboolean         seahorse_source_load_finish          (SeahorseSource *source,
                                                       GAsyncResult *result,
                                                       GError **error);

void             seahorse_source_search_async         (SeahorseSource *source,
                                                       const gchar *match,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

GList *          seahorse_source_search_finish        (SeahorseSource *source,
                                                       GAsyncResult *result,
                                                       GError **error);

void             seahorse_source_import_async         (SeahorseSource *source,
                                                       GInputStream *input,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

GList *          seahorse_source_import_finish        (SeahorseSource *source,
                                                       GAsyncResult *result,
                                                       GError **error);

void             seahorse_source_export_async         (SeahorseSource *source,
                                                       GList *objects,
                                                       GOutputStream *output,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

GOutputStream *  seahorse_source_export_finish        (SeahorseSource *source,
                                                       GAsyncResult *result,
                                                       GError **error);

void             seahorse_source_export_auto_async    (GList *objects,
                                                       GOutputStream *output,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

GOutputStream *  seahorse_source_export_auto_finish   (GAsyncResult *result,
                                                       GError **error);

gboolean         seahorse_source_export_auto_wait     (GList *objects,
                                                       GOutputStream *output,
                                                       GError **error);

void             seahorse_source_export_raw_async     (SeahorseSource *source,
                                                       GList *ids,
                                                       GOutputStream *output,
                                                       GCancellable *cancellable,
                                                       GAsyncReadyCallback callback,
                                                       gpointer user_data);

GOutputStream *  seahorse_source_export_raw_finish    (SeahorseSource *source,
                                                       GAsyncResult *result,
                                                       GError **error);

GQuark           seahorse_source_get_tag              (SeahorseSource *source);

SeahorseLocation seahorse_source_get_location         (SeahorseSource *source);

#endif /* __SEAHORSE_SOURCE_H__ */
