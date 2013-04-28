/*
 * Seahorse
 *
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-data.h"
#include "seahorse-gpgme-exporter.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-gpg-op.h"
#include "seahorse-progress.h"

#include "seahorse-common.h"
#include "seahorse-object.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

#define SEAHORSE_GPGME_EXPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GPGME_EXPORTER, SeahorseGpgmeExporterClass))
#define SEAHORSE_IS_GPGME_EXPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GPGME_EXPORTER))
#define SEAHORSE_GPGME_EXPORTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GPGME_EXPORTER, SeahorseGpgmeExporterClass))

typedef struct _SeahorseGpgmeExporterClass SeahorseGpgmeExporterClass;

struct _SeahorseGpgmeExporter {
	GObject parent;
	GList *objects;
	gboolean armor;
	gboolean secret;
};

struct _SeahorseGpgmeExporterClass {
	GObjectClass parent_class;
};

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_CONTENT_TYPE,
	PROP_FILE_FILTER,
	PROP_ARMOR,
	PROP_SECRET
};

static void   seahorse_gpgme_exporter_iface_init    (SeahorseExporterIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGpgmeExporter, seahorse_gpgme_exporter, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_EXPORTER, seahorse_gpgme_exporter_iface_init);
);

static gchar *
seahorse_gpgme_exporter_get_filename (SeahorseExporter* exporter)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (exporter);
	const gchar *basename = NULL;
	gchar *filename;

	g_return_val_if_fail (self->objects, NULL);

	/* Multiple objects */
	if (self->objects->next)
		basename = _("Multiple Keys");
	else if (self->objects->data)
		basename = seahorse_object_get_nickname (self->objects->data);
	if (basename == NULL)
		basename = _("Key Data");

	if (self->armor)
		filename = g_strconcat (basename, ".asc", NULL);
	else
		filename = g_strconcat (basename, ".pgp", NULL);
	g_strstrip (filename);
	g_strdelimit (filename, SEAHORSE_BAD_FILENAME_CHARS, '_');
	return filename;
}

static const gchar *
seahorse_gpgme_exporter_get_content_type (SeahorseExporter* exporter)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (exporter);
	if (self->armor)
		return "application/pgp-keys";
	else
		return "application/pgp-keys+armor";
}

static GtkFileFilter *
seahorse_gpgme_exporter_get_file_filter (SeahorseExporter* exporter)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (exporter);
	GtkFileFilter *filter = gtk_file_filter_new ();

	if (self->armor) {
		gtk_file_filter_set_name (filter, _("Armored PGP keys"));
		gtk_file_filter_add_mime_type (filter, "application/pgp-keys+armor");
		gtk_file_filter_add_pattern (filter, "*.asc");
	} else {
		gtk_file_filter_set_name (filter, _("PGP keys"));
		gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
		gtk_file_filter_add_pattern (filter, "*.pgp");
		gtk_file_filter_add_pattern (filter, "*.gpg");
	}

	return filter;
}

static void
seahorse_gpgme_exporter_init (SeahorseGpgmeExporter *self)
{

}

static void
seahorse_gpgme_exporter_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (object);
	SeahorseExporter *exporter = SEAHORSE_EXPORTER (object);

	switch (prop_id) {
	case PROP_FILENAME:
		g_value_take_string (value, seahorse_gpgme_exporter_get_filename (exporter));
		break;
	case PROP_CONTENT_TYPE:
		g_value_set_string (value, seahorse_gpgme_exporter_get_content_type (exporter));
		break;
	case PROP_FILE_FILTER:
		g_value_take_object (value, seahorse_gpgme_exporter_get_file_filter (exporter));
		break;
	case PROP_ARMOR:
		g_value_set_boolean (value, self->armor);
		break;
	case PROP_SECRET:
		g_value_set_boolean (value, self->secret);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_gpgme_exporter_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (object);

	switch (prop_id) {
	case PROP_ARMOR:
		self->armor = g_value_get_boolean (value);
		g_object_notify (G_OBJECT (self), "filename");
		g_object_notify (G_OBJECT (self), "file-filter");
		g_object_notify (G_OBJECT (self), "content-type");
		break;
	case PROP_SECRET:
		self->secret = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_gpgme_exporter_finalize (GObject *obj)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (obj);

	g_list_free_full (self->objects, g_object_unref);

	G_OBJECT_CLASS (seahorse_gpgme_exporter_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_exporter_class_init (SeahorseGpgmeExporterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = seahorse_gpgme_exporter_finalize;
	gobject_class->set_property = seahorse_gpgme_exporter_set_property;
	gobject_class->get_property = seahorse_gpgme_exporter_get_property;

	g_object_class_override_property (gobject_class, PROP_FILENAME, "filename");
	g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");
	g_object_class_override_property (gobject_class, PROP_FILE_FILTER, "file-filter");

	g_object_class_install_property (gobject_class, PROP_ARMOR,
	           g_param_spec_boolean ("armor", "Armor", "Armor encoding",
	                                 FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_SECRET,
	           g_param_spec_boolean ("secret", "Secret", "Secret key export",
	                                 FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static GList *
seahorse_gpgme_exporter_get_objects (SeahorseExporter *exporter)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (exporter);
	return self->objects;
}

static gboolean
seahorse_gpgme_exporter_add_object (SeahorseExporter *exporter,
                                    GObject *object)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (exporter);
	SeahorseGpgmeKey *key;

	if (SEAHORSE_IS_GPGME_KEY (object)) {
		key = SEAHORSE_GPGME_KEY (object);
		if (self->secret && seahorse_gpgme_key_get_private (key) == NULL)
			return FALSE;

		self->objects = g_list_append (self->objects, g_object_ref (key));
		g_object_notify (G_OBJECT (self), "filename");
		return TRUE;
	}

	return FALSE;
}

typedef struct {
	GPtrArray *keyids;
	gint at;
	gpgme_data_t data;
	gpgme_ctx_t gctx;
	GMemoryOutputStream *output;
	GCancellable *cancellable;
	gulong cancelled_sig;
} GpgmeExportClosure;

static void
gpgme_export_closure_free (gpointer data)
{
	GpgmeExportClosure *closure = data;
	g_cancellable_disconnect (closure->cancellable, closure->cancelled_sig);
	g_clear_object (&closure->cancellable);
	gpgme_data_release (closure->data);
	if (closure->gctx)
		gpgme_release (closure->gctx);
	g_ptr_array_free (closure->keyids, TRUE);
	g_object_unref (closure->output);
	g_free (closure);
}


static gboolean
on_keyring_export_complete (gpgme_error_t gerr,
                            gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	GpgmeExportClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		return FALSE; /* don't call again */
	}

	if (closure->at >= 0)
		seahorse_progress_end (closure->cancellable,
		                       closure->keyids->pdata[closure->at]);

	g_assert (closure->at < (gint)closure->keyids->len);
	closure->at++;

	if (closure->at == (gint)closure->keyids->len) {
		g_simple_async_result_complete (res);
		return FALSE; /* don't run this again */
	}

	/* Do the next key in the list */
	gerr = gpgme_op_export_start (closure->gctx, closure->keyids->pdata[closure->at],
	                              0, closure->data);

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
		return FALSE; /* don't run this again */
	}

	seahorse_progress_begin (closure->cancellable,
	                         closure->keyids->pdata[closure->at]);
	return TRUE; /* call this source again */
}

static void
seahorse_gpgme_exporter_export_async (SeahorseExporter *exporter,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
	SeahorseGpgmeExporter *self = SEAHORSE_GPGME_EXPORTER (exporter);
	GSimpleAsyncResult *res;
	GpgmeExportClosure *closure;
	GError *error = NULL;
	gpgme_error_t gerr = 0;
	SeahorsePgpKey *key;
	gchar *keyid;
	GSource *gsource;
	GList *l;

	res = g_simple_async_result_new (G_OBJECT (exporter), callback, user_data,
	                                 seahorse_gpgme_exporter_export_async);
	closure = g_new0 (GpgmeExportClosure, 1);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	closure->gctx = seahorse_gpgme_keyring_new_context (&gerr);
	closure->output = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0, g_realloc, g_free));
	closure->data = seahorse_gpgme_data_output (G_OUTPUT_STREAM (closure->output));
	closure->keyids = g_ptr_array_new_with_free_func (g_free);
	closure->at = -1;
	g_simple_async_result_set_op_res_gpointer (res, closure, gpgme_export_closure_free);

	if (seahorse_gpgme_propagate_error (gerr, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	gpgme_set_armor (closure->gctx, self->armor);

	for (l = self->objects; l != NULL; l = g_list_next (l)) {
		key = SEAHORSE_PGP_KEY (l->data);

		/* Building list */
		keyid = g_strdup (seahorse_pgp_key_get_keyid (key));
		seahorse_progress_prep (closure->cancellable, keyid, NULL);
		g_ptr_array_add (closure->keyids, keyid);
	}

	if (self->secret) {
		g_return_if_fail (self->armor == TRUE);
		g_ptr_array_add (closure->keyids, NULL);
		gerr = seahorse_gpg_op_export_secret (closure->gctx, (const gchar **)closure->keyids->pdata,
		                                      closure->data);
		if (seahorse_gpgme_propagate_error (gerr, &error))
			g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		g_object_unref (res);
		return;
	}

	gsource = seahorse_gpgme_gsource_new (closure->gctx, cancellable);
	g_source_set_callback (gsource, (GSourceFunc)on_keyring_export_complete,
	                       g_object_ref (res), g_object_unref);

	/* Get things started */
	if (on_keyring_export_complete (0, res))
		g_source_attach (gsource, g_main_context_default ());

	g_source_unref (gsource);
	g_object_unref (res);
}

static guchar *
seahorse_gpgme_exporter_export_finish (SeahorseExporter *exporter,
                                       GAsyncResult *result,
                                       gsize *size,
                                       GError **error)
{
	GpgmeExportClosure *closure;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (exporter),
	                      seahorse_gpgme_exporter_export_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	closure = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));
	g_output_stream_close (G_OUTPUT_STREAM (closure->output), NULL, NULL);
	*size = g_memory_output_stream_get_data_size (closure->output);
	return g_memory_output_stream_steal_data (closure->output);
}

static void
seahorse_gpgme_exporter_iface_init (SeahorseExporterIface *iface)
{
	iface->add_object = seahorse_gpgme_exporter_add_object;
	iface->export = seahorse_gpgme_exporter_export_async;
	iface->export_finish = seahorse_gpgme_exporter_export_finish;
	iface->get_objects = seahorse_gpgme_exporter_get_objects;
	iface->get_filename = seahorse_gpgme_exporter_get_filename;
	iface->get_content_type = seahorse_gpgme_exporter_get_content_type;
	iface->get_file_filter = seahorse_gpgme_exporter_get_file_filter;
}

SeahorseExporter *
seahorse_gpgme_exporter_new (GObject *object,
                             gboolean armor,
                             gboolean secret)
{
	SeahorseExporter *exporter;

	g_return_val_if_fail (secret == FALSE || armor == TRUE, NULL);

	exporter = g_object_new (SEAHORSE_TYPE_GPGME_EXPORTER,
	                         "armor", armor,
	                         "secret", secret,
	                         NULL);

	if (!seahorse_exporter_add_object (exporter, object))
		g_return_val_if_reached (NULL);

	return exporter;
}


SeahorseExporter *
seahorse_gpgme_exporter_new_multiple (GList *keys,
                                      gboolean armor)
{
	SeahorseExporter *exporter;
	GList *l;

	exporter = g_object_new (SEAHORSE_TYPE_GPGME_EXPORTER,
	                         "armor", armor,
	                         "secret", FALSE,
	                         NULL);

	for (l = keys; l != NULL; l = g_list_next (l)) {
		if (!seahorse_exporter_add_object (exporter, l->data))
			g_return_val_if_reached (NULL);
	}

	return exporter;
}
