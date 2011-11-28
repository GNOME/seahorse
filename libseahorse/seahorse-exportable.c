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

#include "seahorse-exportable.h"
#include "seahorse-exporter.h"

#include <glib/gi18n.h>

#include <string.h>

typedef SeahorseExportableIface SeahorseExportableInterface;

G_DEFINE_INTERFACE (SeahorseExportable, seahorse_exportable, G_TYPE_OBJECT);

GList *
seahorse_exportable_create_exporters (SeahorseExportable *exportable,
                                      SeahorseExporterType type)
{
	SeahorseExportableIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_EXPORTABLE (exportable), NULL);

	iface = SEAHORSE_EXPORTABLE_GET_INTERFACE (exportable);
	g_return_val_if_fail (iface->create_exporters, NULL);

	return iface->create_exporters (exportable, type);
}

typedef struct {
	GMainLoop *loop;
	GAsyncResult *result;
} WaitClosure;

static WaitClosure *
wait_closure_new (void)
{
	WaitClosure *closure;

	closure = g_slice_new0 (WaitClosure);
	closure->loop = g_main_loop_new (NULL, FALSE);

	return closure;
}

static void
wait_closure_free (WaitClosure *closure)
{
	g_clear_object (&closure->result);
	g_main_loop_unref (closure->loop);
	g_slice_free (WaitClosure, closure);
}

static void
on_wait_complete (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
	WaitClosure *closure = user_data;
	g_assert (closure->result == NULL);
	closure->result = g_object_ref (result);
	g_main_loop_quit (closure->loop);
}

static void
seahorse_exportable_default_init (SeahorseExportableIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		initialized = TRUE;
	}
}

guint
seahorse_exportable_export_to_directory_wait (GList *objects,
                                              const gchar *directory,
                                              GError **error)
{
	SeahorseExporter *exporter;
	WaitClosure *closure;
	GList *exporters;
	guint count = 0;
	gboolean ret;
	gchar *filename;
	gchar *name;
	GFile *file;
	GList *l;

	g_return_val_if_fail (directory != NULL, 0);
	g_return_val_if_fail (error == NULL || *error == NULL, 0);

	closure = wait_closure_new ();

	for (l = objects; l != NULL; l = g_list_next (l)) {
		if (!SEAHORSE_IS_EXPORTABLE (l->data))
			continue;

		exporters = seahorse_exportable_create_exporters (SEAHORSE_EXPORTABLE (l->data),
		                                                  SEAHORSE_EXPORTER_ANY);
		if (!exporters)
			continue;

		exporter = g_object_ref (exporters->data);
		g_list_free_full (exporters, g_object_unref);

		name = seahorse_exporter_get_filename (exporter);
		filename = g_build_filename (directory, name, NULL);
		g_free (name);

		file = g_file_new_for_uri (filename);
		seahorse_exporter_export_to_file_async (exporter, file, FALSE,
		                                       NULL, on_wait_complete, closure);
		g_object_unref (file);

		g_main_loop_run (closure->loop);

		ret = seahorse_exporter_export_to_file_finish (exporter, closure->result, error);
		g_object_unref (closure->result);
		closure->result = NULL;

		g_object_unref (exporter);
		g_free (filename);

		if (ret)
			count++;
		else
			break;
	}

	wait_closure_free (closure);
	return count;
}

guint
seahorse_exportable_export_to_text_wait (GList *objects,
                                         gpointer *data,
                                         gsize *size,
                                         GError **error)
{
	SeahorseExporter *exporter = NULL;
	WaitClosure *closure;
	GList *exporters = NULL;
	guint count, total;
	GList *l, *e;

	g_return_val_if_fail (error == NULL || *error == NULL, 0);
	g_return_val_if_fail (data != NULL, 0);
	g_return_val_if_fail (size != NULL, 0);

	for (l = objects; l != NULL; l = g_list_next (l)) {
		if (!SEAHORSE_IS_EXPORTABLE (l->data))
			continue;

		/* If we've already found exporters, then add to those */
		if (exporters) {
			for (e = exporters; e != NULL; e = g_list_next (e))
				seahorse_exporter_add_object (e->data, l->data);

		/* Otherwise try and create new exporters for this object */
		} else {
			exporters = seahorse_exportable_create_exporters (SEAHORSE_EXPORTABLE (l->data),
			                                                  SEAHORSE_EXPORTER_TEXTUAL);
		}
	}

	/* Find the exporter than has the most objects */
	for (e = exporters, total = 0, exporter = NULL;
	     e != NULL; e = g_list_next (e)) {
		count = g_list_length (seahorse_exporter_get_objects (e->data));
		if (count > total) {
			total = count;
			g_clear_object (&exporter);
			exporter = g_object_ref (e->data);
		}
	}

	g_list_free_full (exporters, g_object_unref);

	if (exporter) {
		closure = wait_closure_new ();

		seahorse_exporter_export_async (exporter, NULL, on_wait_complete, closure);

		g_main_loop_run (closure->loop);

		*data = seahorse_exporter_export_finish (exporter, closure->result, size, error);
		if (*data == NULL)
			total = 0;

		wait_closure_free (closure);
		g_object_unref (exporter);
	}

	return total;
}

guint
seahorse_exportable_export_to_prompt_wait (GList *objects,
                                           GtkWindow *parent,
                                           GError **error)
{
	SeahorseExporter *exporter;
	gchar *directory = NULL;
	WaitClosure *closure;
	GHashTable *pending;
	GList *exporters;
	guint count = 0;
	gboolean ret;
	GList *l, *e, *x;
	GList *exported;
	GFile *file;

	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), 0);
	g_return_val_if_fail (error == NULL || *error == NULL, 0);

	/* A table for monitoring which objects are still pending */
	pending = g_hash_table_new (g_direct_hash, g_direct_equal);
	for (l = objects; l != NULL; l = g_list_next (l))
		g_hash_table_replace (pending, l->data, l->data);

	closure = wait_closure_new ();

	for (l = objects; l != NULL; l = g_list_next (l)) {
		if (!g_hash_table_lookup (pending, l->data))
			continue;

		if (!SEAHORSE_IS_EXPORTABLE (l->data)) {
			g_hash_table_remove (pending, l->data);
			continue;
		}

		exporters = seahorse_exportable_create_exporters (SEAHORSE_EXPORTABLE (l->data),
		                                                  SEAHORSE_EXPORTER_ANY);
		if (!exporters)
			continue;

		/* Now go and add all pending to each exporter */
		for (x = objects; x != NULL; x = g_list_next (x)) {
			if (x->data == l->data)
				continue;
			if (g_hash_table_lookup (pending, x->data)) {
				for (e = exporters; e != NULL; e = g_list_next (e))
					seahorse_exporter_add_object (e->data, x->data);
			}
		}

		/* Now show a prompt choosing between the exporters */
		ret = seahorse_exportable_prompt (exporters, parent,
		                                  &directory, &file, &exporter);

		g_list_free_full (exporters, g_object_unref);

		if (!ret)
			break;

		seahorse_exporter_export_to_file_async (exporter, file, TRUE,
		                                        NULL, on_wait_complete, closure);

		g_main_loop_run (closure->loop);

		ret = seahorse_exporter_export_to_file_finish (exporter, closure->result, error);
		g_object_unref (closure->result);
		closure->result = NULL;

		if (ret) {
			exported = seahorse_exporter_get_objects (exporter);
			for (e = exported; e != NULL; e = g_list_next (e)) {
				g_hash_table_remove (pending, e->data);
				count++;
			}
		}

		g_object_unref (file);
		g_object_unref (exporter);

		if (!ret)
			break;
	}

	g_free (directory);
	wait_closure_free (closure);
	g_hash_table_destroy (pending);
	return count;

}

static gchar *
calculate_basename (GFile *file,
                    const gchar *extension)
{
	gchar *basename = g_file_get_basename (file);
	gchar *name;
	gchar *cut;

	cut = strrchr (basename, '.');
	if (cut != NULL)
		cut[0] = '\0';

	name = g_strdup_printf ("%s%s", basename, extension);
	g_free (basename);

	return name;
}

static void
on_filter_changed (GObject *obj,
                   GParamSpec *pspec,
                   gpointer user_data)
{
	GHashTable *filters = user_data;
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (obj);
	SeahorseExporter *exporter;
	GtkFileFilter *filter;
	const gchar *extension;
	gchar *basename;
	gchar *name;
	GFile *file;

	filter = gtk_file_chooser_get_filter (chooser);
	g_return_if_fail (filter != NULL);

	exporter = g_hash_table_lookup (filters, filter);
	g_return_if_fail (exporter != NULL);

	name = seahorse_exporter_get_filename (exporter);
	g_return_if_fail (name != NULL);

	extension = strrchr (name, '.');
	if (extension) {
		file = gtk_file_chooser_get_file (chooser);
		if (file) {
			basename = calculate_basename (file, extension);
			g_object_unref (file);
			if (basename)
				gtk_file_chooser_set_current_name (chooser, basename);
			g_free (basename);
		} else {
			gtk_file_chooser_set_current_name (chooser, name);
		}
	}

	g_free (name);
}

gboolean
seahorse_exportable_prompt (GList *exporters,
                            GtkWindow *parent,
                            gchar **directory,
                            GFile **file,
                            SeahorseExporter **exporter)
{
	GtkWidget *widget;
	GtkDialog *dialog;
	GtkFileChooser *chooser;
	GHashTable *filters;
	GtkFileFilter *first = NULL;
	GtkFileFilter *filter;
	gboolean result = FALSE;
	GList *e;

	g_return_val_if_fail (exporters != NULL, FALSE);
	g_return_val_if_fail (parent == NULL || GTK_WINDOW (parent), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);
	g_return_val_if_fail (exporter != NULL, FALSE);

	widget = gtk_file_chooser_dialog_new (NULL, parent, GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                      _("Export"), GTK_RESPONSE_ACCEPT,
	                                      NULL);

	dialog = GTK_DIALOG (widget);
	chooser = GTK_FILE_CHOOSER (widget);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_local_only (chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

	if (directory && *directory != NULL)
		gtk_file_chooser_set_current_folder (chooser, *directory);

	filters = g_hash_table_new (g_direct_hash, g_direct_equal);
	for (e = exporters; e != NULL; e = g_list_next (e)) {
		filter = seahorse_exporter_get_file_filter (e->data);
		g_hash_table_replace (filters, filter, e->data);
		gtk_file_chooser_add_filter (chooser, filter);
		if (first == NULL)
			first = filter;
	}

	g_signal_connect (chooser, "notify::filter", G_CALLBACK (on_filter_changed), filters);
	gtk_file_chooser_set_filter (chooser, first);

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
		*file = gtk_file_chooser_get_file (chooser);
		filter = gtk_file_chooser_get_filter (chooser);
		*exporter = g_object_ref (g_hash_table_lookup (filters, filter));
		if (directory) {
			g_free (*directory);
			*directory = g_strdup (gtk_file_chooser_get_current_folder (chooser));
		}
		result = TRUE;
	}

	g_hash_table_destroy (filters);
	gtk_widget_destroy (widget);
	return result;
}
