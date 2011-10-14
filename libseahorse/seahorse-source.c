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
#include "seahorse-registry.h"
#include "seahorse-source.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

/**
 * SECTION:seahorse-source
 * @short_description: This class stores and handles key sources
 * @include:seahorse-source.h
 *
 **/

typedef SeahorseSourceIface SeahorseSourceInterface;

G_DEFINE_INTERFACE (SeahorseSource, seahorse_source, GCR_TYPE_COLLECTION);

/* ---------------------------------------------------------------------------------
 * INTERFACE
 */

/**
* gobject_class: The object class to init
*
**/
static void
seahorse_source_default_init (SeahorseSourceIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		g_object_interface_install_property (iface,
		           g_param_spec_string ("label", "Label", "Label for the source",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_string ("description", "Description", "Description for the source",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_string ("uri", "URI", "URI for the source",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_object ("icon", "Icon", "Icon for this source",
		                                G_TYPE_ICON, G_PARAM_READABLE));

		initialized = TRUE;
	}
}

/* ---------------------------------------------------------------------------------
 * PUBLIC
 */

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

	g_return_if_fail (SEAHORSE_IS_SOURCE (source));
	g_return_if_fail (G_IS_OUTPUT_STREAM (output));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = SEAHORSE_SOURCE_GET_INTERFACE (source);
	g_return_if_fail (iface->export_async != NULL);

	iface->export_async (source, objects, output,
	                     cancellable, callback, user_data);
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
	g_return_val_if_fail (iface->export_async != NULL, NULL);
	g_return_val_if_fail (iface->export_finish, NULL);
	return (iface->export_finish) (source, result, error);
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
	GObject *object;
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

		g_assert (G_IS_OBJECT (objects->data));
		object = G_OBJECT (objects->data);

		/* Export from this object source */
		source = NULL;
		g_object_get (object, "source", &source, NULL);
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
