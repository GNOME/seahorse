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

#include "seahorse-exporter.h"
#include "seahorse-util.h"

typedef SeahorseExporterIface SeahorseExporterInterface;

G_DEFINE_INTERFACE (SeahorseExporter, seahorse_exporter, G_TYPE_OBJECT);

static void
seahorse_exporter_default_init (SeahorseExporterIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		g_object_interface_install_property (iface,
		                g_param_spec_string ("filename", "File name", "File name",
		                                     NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                g_param_spec_string ("content-type", "Content type", "Content type",
		                                     NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		                g_param_spec_object ("file-filter", "File filter", "File chooser filter",
		                                     GTK_TYPE_FILE_FILTER, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		initialized = TRUE;
	}
}

gchar *
seahorse_exporter_get_filename (SeahorseExporter *exporter)
{
	gchar *filename = NULL;

	g_return_val_if_fail (SEAHORSE_IS_EXPORTER (exporter), NULL);

	g_object_get (exporter, "filename", &filename, NULL);
	return filename;
}

gchar *
seahorse_exporter_get_content_type (SeahorseExporter *exporter)
{
	gchar *content_type = NULL;

	g_return_val_if_fail (SEAHORSE_IS_EXPORTER (exporter), NULL);

	g_object_get (exporter, "content-type", &content_type, NULL);
	return content_type;
}

GtkFileFilter *
seahorse_exporter_get_file_filter (SeahorseExporter *exporter)
{
	GtkFileFilter *filter = NULL;

	g_return_val_if_fail (SEAHORSE_IS_EXPORTER (exporter), NULL);

	g_object_get (exporter, "file-filter", &filter, NULL);
	return filter;
}

GList *
seahorse_exporter_get_objects (SeahorseExporter *exporter)
{
	SeahorseExporterIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_EXPORTER (exporter), NULL);

	iface = SEAHORSE_EXPORTER_GET_INTERFACE (exporter);
	g_return_val_if_fail (iface->get_objects != NULL, NULL);

	return (iface->get_objects) (exporter);
}

gboolean
seahorse_exporter_add_object (SeahorseExporter *exporter,
                              GObject *object)
{
	SeahorseExporterIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_EXPORTER (exporter), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

	iface = SEAHORSE_EXPORTER_GET_INTERFACE (exporter);
	g_return_val_if_fail (iface->add_object != NULL, FALSE);

	return (iface->add_object) (exporter, object);
}

void
seahorse_exporter_export_async (SeahorseExporter *exporter,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	SeahorseExporterIface *iface;

	g_return_if_fail (SEAHORSE_IS_EXPORTER (exporter));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = SEAHORSE_EXPORTER_GET_INTERFACE (exporter);
	g_return_if_fail (iface->export_async != NULL);

	(iface->export_async) (exporter, cancellable, callback, user_data);
}

gpointer
seahorse_exporter_export_finish (SeahorseExporter *exporter,
                                 GAsyncResult *result,
                                 gsize *size,
                                 GError **error)
{
	SeahorseExporterIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_EXPORTER (exporter), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	g_return_val_if_fail (size != NULL, NULL);

	iface = SEAHORSE_EXPORTER_GET_INTERFACE (exporter);
	g_return_val_if_fail (iface->export_finish != NULL, NULL);

	return (iface->export_finish) (exporter, result, size, error);
}

typedef struct {
	GCancellable *cancellable;
	gboolean overwrite;
	guint unique;
	gpointer data;
	gsize size;
	GFile *file;
} FileClosure;

static void
file_closure_free (gpointer data)
{
	FileClosure *closure = data;
	g_clear_object (&closure->cancellable);
	g_object_unref (closure->file);
	g_free (closure->data);
	g_free (closure);
}

static void
on_export_replace (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	FileClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	GFile *file;

	if (g_file_replace_contents_finish (closure->file, result, NULL, &error)) {
		g_simple_async_result_complete (res);

	/* Not overwriting, and the file existed, try another file name */
	} else if (!closure->overwrite && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WRONG_ETAG)) {
		file = seahorse_util_file_increment_unique (closure->file, &closure->unique);

		g_file_replace_contents_async (file, closure->data, closure->size, "invalid-etag",
		                               FALSE, G_FILE_CREATE_PRIVATE, closure->cancellable,
		                               on_export_replace, g_object_ref (res));

		g_clear_error (&error);
		g_object_unref (file);

	} else {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	}

	g_object_unref (res);
}

static void
on_export_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	FileClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	closure->data = seahorse_exporter_export_finish (SEAHORSE_EXPORTER (source),
	                                                 result, &closure->size, &error);
	if (error == NULL) {
		/*
		 * When not trying to overwrite we pass an invalid etag. This way
		 * if the file exists, it will not match the etag, and we'll be
		 * able to detect it and try another file name.
		 */
		g_file_replace_contents_async (closure->file, closure->data,
		                               closure->size, closure->overwrite ? NULL : "invalid-etag",
		                               FALSE, G_FILE_CREATE_PRIVATE, closure->cancellable,
		                               on_export_replace, g_object_ref (res));
	} else {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	}

	g_object_unref (res);
}

void
seahorse_exporter_export_to_file_async (SeahorseExporter *exporter,
                                        GFile *file,
                                        gboolean overwrite,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data)
{
	GSimpleAsyncResult *res;
	FileClosure *closure;

	g_return_if_fail (SEAHORSE_IS_EXPORTER (exporter));
	g_return_if_fail (G_IS_FILE (file));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	res = g_simple_async_result_new (G_OBJECT (exporter), callback, user_data,
	                                 seahorse_exporter_export_to_file_async);
	closure = g_new0 (FileClosure, 1);
	closure->file = g_object_ref (file);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->overwrite = overwrite;
	g_simple_async_result_set_op_res_gpointer (res, closure, file_closure_free);

	seahorse_exporter_export_async (exporter, cancellable, on_export_complete,
	                                g_object_ref (res));

	g_object_unref (res);
}

gboolean
seahorse_exporter_export_to_file_finish (SeahorseExporter *exporter,
                                         GAsyncResult *result,
                                         GError **error)
{
	g_return_val_if_fail (SEAHORSE_IS_EXPORTER (exporter), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (exporter),
	                      seahorse_exporter_export_to_file_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}
