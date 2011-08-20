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
 
#include "config.h"

#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-object.h"
#include "seahorse-source.h"
#include "seahorse-util.h"

#include "common/seahorse-registry.h"

/**
 * SECTION:seahorse-source
 * @short_description: This class stores and handles key sources
 * @include:seahorse-source.h
 *
 **/


/* ---------------------------------------------------------------------------------
 * INTERFACE
 */

/**
* gobject_class: The object class to init
*
* Adds the interfaces "source-tag" and "source-location"
*
**/
static void
seahorse_source_base_init (gpointer gobject_class)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		
		/* Add properties and signals to the interface */
		g_object_interface_install_property (gobject_class,
		        g_param_spec_uint ("source-tag", "Source Tag", "Tag of objects that come from this source.", 
		                           0, G_MAXUINT, SEAHORSE_TAG_INVALID, G_PARAM_READABLE));

		g_object_interface_install_property (gobject_class, 
		        g_param_spec_enum ("source-location", "Source Location", "Objects in this source are at this location. See SeahorseLocation", 
		                           SEAHORSE_TYPE_LOCATION, SEAHORSE_LOCATION_LOCAL, G_PARAM_READABLE));
		
		initialized = TRUE;
	}
}

/**
 * seahorse_source_get_type:
 *
 * Registers the type of G_TYPE_INTERFACE
 *
 * Returns: the type id
 */
GType
seahorse_source_get_type (void)
{
	static GType type = 0;
	if (!type) {
		static const GTypeInfo info = {
			sizeof (SeahorseSourceIface),
			seahorse_source_base_init,               /* base init */
			NULL,             /* base finalize */
			NULL,             /* class_init */
			NULL,             /* class finalize */
			NULL,             /* class data */
			0,
			0,                /* n_preallocs */
			NULL,             /* instance init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE, "SeahorseSourceIface", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	
	return type;
}

/* ---------------------------------------------------------------------------------
 * PUBLIC
 */

void
seahorse_source_load_async (SeahorseSource *source,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
	g_return_if_fail (SEAHORSE_IS_SOURCE (source));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (SEAHORSE_SOURCE_GET_INTERFACE (source)->load_async);
	SEAHORSE_SOURCE_GET_INTERFACE (source)->load_async (source, cancellable,
	                                                  callback, user_data);
}

gboolean
seahorse_source_load_finish (SeahorseSource *source,
                             GAsyncResult *result,
                             GError **error)
{
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (SEAHORSE_SOURCE_GET_INTERFACE (source)->load_finish, FALSE);
	return SEAHORSE_SOURCE_GET_INTERFACE (source)->load_finish (source, result, error);
}

void
seahorse_source_search_async (SeahorseSource *source,
                              const gchar *match,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	g_return_if_fail (SEAHORSE_IS_SOURCE (source));
	g_return_if_fail (match != NULL);
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (SEAHORSE_SOURCE_GET_INTERFACE (source)->search_async);
	SEAHORSE_SOURCE_GET_INTERFACE (source)->search_async (source, match, cancellable,
	                                                    callback, user_data);
}

GList *
seahorse_source_search_finish (SeahorseSource *source, GAsyncResult *result,
                               GError **error)
{
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (source), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	g_return_val_if_fail (SEAHORSE_SOURCE_GET_INTERFACE (source)->search_finish, NULL);
	return SEAHORSE_SOURCE_GET_INTERFACE (source)->search_finish (source, result, error);
}

void
seahorse_source_import_async (SeahorseSource *source,
                              GInputStream *input,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	g_return_if_fail (SEAHORSE_IS_SOURCE (source));
	g_return_if_fail (G_IS_INPUT_STREAM (input));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (SEAHORSE_SOURCE_GET_INTERFACE (source)->import_async);
	SEAHORSE_SOURCE_GET_INTERFACE (source)->import_async (source, input, cancellable,
	                                                    callback, user_data);
}

GList *
seahorse_source_import_finish (SeahorseSource *source,
                               GAsyncResult *result,
                               GError **error)
{
	g_return_val_if_fail (SEAHORSE_IS_SOURCE (source), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	g_return_val_if_fail (SEAHORSE_SOURCE_GET_INTERFACE (source)->import_finish, NULL);
	return SEAHORSE_SOURCE_GET_INTERFACE (source)->import_finish (source, result, error);
}

void
seahorse_source_export_async (SeahorseSource *source,
                              GList *objects,
                              GOutputStream *output,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	SeahorseSourceIface *iface;
	GList *l;
	GList *ids;

	g_return_if_fail (SEAHORSE_IS_SOURCE (source));
	g_return_if_fail (G_IS_OUTPUT_STREAM (output));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = SEAHORSE_SOURCE_GET_INTERFACE (source);
	g_return_if_fail (iface->export_async != NULL || iface->export_raw_async != NULL);

	if (iface->export_async) {
		iface->export_async (source, objects, output,
		                     cancellable, callback, user_data);
		return;
	}

	for (l = objects; l != NULL; l = g_list_next (l))
		ids = g_list_prepend (ids, GUINT_TO_POINTER (seahorse_object_get_id (l->data)));

	ids = g_list_reverse (ids);
	(iface->export_raw_async) (source, ids, output, cancellable, callback, user_data);
	g_list_free (ids);
}

GOutputStream *
seahorse_source_export_finish (SeahorseSource *source,
                               GAsyncResult *result,
                               GError **error)
{
	SeahorseSourceIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_SOURCE (source), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	iface = SEAHORSE_SOURCE_GET_INTERFACE (source);
	g_return_val_if_fail (iface->export_async != NULL || iface->export_raw_async != NULL, NULL);

	if (iface->export_async) {
		g_return_val_if_fail (iface->export_finish, NULL);
		return (iface->export_finish) (source, result, error);
	} else {
		g_return_val_if_fail (iface->export_raw_finish, NULL);
		return (iface->export_raw_finish) (source, result, error);
	}
}

typedef struct {
	GOutputStream *output;
	guint exports;
} export_auto_closure;

static void
export_auto_free (gpointer data)
{
	export_auto_closure *closure = data;
	g_object_unref (closure->output);
	g_free (closure);
}

static void
on_export_auto_complete (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	export_auto_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	if (!seahorse_source_export_finish (SEAHORSE_SOURCE (source), result, &error))
		g_simple_async_result_take_error (res, error);

	g_assert (closure->exports > 0);
	closure->exports--;

	if (closure->exports == 0)
		g_simple_async_result_complete (res);

	g_object_unref (res);
}

void
seahorse_source_export_auto_async (GList *objects,
                                   GOutputStream *output,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	GSimpleAsyncResult *res;
	export_auto_closure *closure;
	SeahorseSource *source;
	SeahorseObject *object;
	GList *next;

	res = g_simple_async_result_new (NULL, callback, user_data,
	                                 seahorse_source_export_auto_async);
	closure = g_new0 (export_auto_closure, 1);
	closure->output = g_object_ref (output);
	g_simple_async_result_set_op_res_gpointer (res, closure, export_auto_free);

	/* Sort by object source */
	objects = g_list_copy (objects);
	objects = seahorse_util_objects_sort (objects);

	while (objects) {

		/* Break off one set (same source) */
		next = seahorse_util_objects_splice (objects);

		g_assert (SEAHORSE_IS_OBJECT (objects->data));
		object = SEAHORSE_OBJECT (objects->data);

		/* Export from this object source */
		source = seahorse_object_get_source (object);
		g_return_if_fail (source != NULL);

		/* We pass our own data object, to which data is appended */
		seahorse_source_export_async (source, objects, output, cancellable,
		                              on_export_auto_complete, g_object_ref (res));
		closure->exports++;

		g_list_free (objects);
		objects = next;
	}

	if (closure->exports == 0)
		g_simple_async_result_complete_in_idle (res);

	g_object_unref (res);
}

GOutputStream *
seahorse_source_export_auto_finish (GAsyncResult *result,
                                    GError **error)
{
	export_auto_closure *closure;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
	                      seahorse_source_export_auto_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	return closure->output;

}

static void
on_export_auto_wait_complete (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
	GAsyncResult **ret = user_data;
	*ret = g_object_ref (result);
}

gboolean
seahorse_source_export_auto_wait (GList *objects,
                                  GOutputStream *output,
                                  GError **error)
{
	GAsyncResult *result = NULL;
	gboolean ret;

	seahorse_source_export_auto_async (objects, output, NULL,
	                                   on_export_auto_wait_complete,
	                                   &result);

	seahorse_util_wait_until (result != NULL);

	ret = seahorse_source_export_auto_finish (result, error) != NULL;
	g_object_unref (result);
	return ret;
}

void
seahorse_source_export_raw_async (SeahorseSource *source,
                                  GList *ids,
                                  GOutputStream *output,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	SeahorseSourceIface *iface;
	SeahorseObject *object;
	GList *objects = NULL;
	GList *l;

	g_return_if_fail (SEAHORSE_IS_SOURCE (source));
	g_return_if_fail (output == NULL || G_IS_OUTPUT_STREAM (output));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = SEAHORSE_SOURCE_GET_INTERFACE (source);

	/* Either export or export_raw must be implemented */
	if (iface->export_raw_async) {
		(iface->export_raw_async) (source, ids, output, cancellable, callback, user_data);
		return;
	}

	g_return_if_fail (iface->export_async != NULL);

	for (l = ids; l != NULL; l = g_list_next (l)) {
		object = seahorse_context_get_object (seahorse_context_instance (),
		                                      source, GPOINTER_TO_UINT (l->data));

		/* TODO: A proper error message here 'not found' */
		if (object != NULL)
			objects = g_list_prepend (objects, object);
	}

	objects = g_list_reverse (objects);
	(iface->export_async) (source, objects, output, cancellable, callback, user_data);
	g_list_free (objects);
}

GOutputStream *
seahorse_source_export_raw_finish (SeahorseSource *source,
                                   GAsyncResult *result,
                                   GError **error)
{
	SeahorseSourceIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_SOURCE (source), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	iface = SEAHORSE_SOURCE_GET_INTERFACE (source);
	g_return_val_if_fail (iface->export_async != NULL || iface->export_raw_async != NULL, NULL);

	if (iface->export_raw_async) {
		g_return_val_if_fail (iface->export_raw_finish, NULL);
		return (iface->export_raw_finish) (source, result, error);
	} else {
		g_return_val_if_fail (iface->export_finish, NULL);
		return (iface->export_finish) (source, result, error);
	}
}

/**
* seahorse_source_get_tag:
* @sksrc: The seahorse source object
*
*
* Returns: The source-tag property of the object. As #GQuark
*/
GQuark              
seahorse_source_get_tag (SeahorseSource *sksrc)
{
    GQuark ktype;
    g_object_get (sksrc, "source-tag", &ktype, NULL);
    return ktype;
}

/**
 * seahorse_source_get_location:
 * @sksrc: The seahorse source object
 *
 *
 *
 * Returns: The location (#SeahorseLocation) of this object
 */
SeahorseLocation   
seahorse_source_get_location (SeahorseSource *sksrc)
{
    SeahorseLocation loc;
    g_object_get (sksrc, "source-location", &loc, NULL);
    return loc;
}
