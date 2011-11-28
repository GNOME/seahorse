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

#include "seahorse-certificate.h"
#include "seahorse-certificate-der-exporter.h"

#include "seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

#include <string.h>

#define SEAHORSE_CERTIFICATE_DER_EXPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CERTIFICATE_DER_EXPORTER, SeahorseCertificateDerExporterClass))
#define SEAHORSE_IS_CERTIFICATE_DER_EXPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CERTIFICATE_DER_EXPORTER))
#define SEAHORSE_CERTIFICATE_DER_EXPORTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_CERTIFICATE_DER_EXPORTER, SeahorseCertificateDerExporterClass))

typedef struct _SeahorseCertificateDerExporterClass SeahorseCertificateDerExporterClass;

struct _SeahorseCertificateDerExporter {
	GObject parent;
	SeahorseCertificate *certificate;
	GList *objects;
};

struct _SeahorseCertificateDerExporterClass {
	GObjectClass parent_class;
};

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_CONTENT_TYPE,
	PROP_FILE_FILTER,
};

static void   seahorse_certificate_der_exporter_iface_init    (SeahorseExporterIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseCertificateDerExporter, seahorse_certificate_der_exporter, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_EXPORTER, seahorse_certificate_der_exporter_iface_init);
);

static gchar *
calc_filename (SeahorseCertificateDerExporter *self)
{
	const gchar *basename = NULL;
	gchar *label = NULL;
	gchar *filename;

	g_return_val_if_fail (self->certificate, NULL);

	g_object_get (self->certificate, "label", &label, NULL);
	if (label == NULL)
		basename = _("Certificate");
	else
		basename = label;

	filename = g_strconcat (basename, ".crt", NULL);
	g_strstrip (filename);
	g_strdelimit (filename, SEAHORSE_BAD_FILENAME_CHARS, '_');
	g_free (label);
	return filename;
}

static const gchar *
calc_content_type (SeahorseCertificateDerExporter *self)
{
	return "application/pkix-cert";
}

static GtkFileFilter *
calc_file_filter (SeahorseCertificateDerExporter *self)
{
	GtkFileFilter *filter = gtk_file_filter_new ();

	gtk_file_filter_set_name (filter, _("Certificates (DER encoded)"));
	gtk_file_filter_add_mime_type (filter, "application/pkix-cert");
	gtk_file_filter_add_mime_type (filter, "application/x-x509-ca-cert");
	gtk_file_filter_add_mime_type (filter, "application/x-x509-user-cert");
	gtk_file_filter_add_pattern (filter, "*.cer");
	gtk_file_filter_add_pattern (filter, "*.crt");
	gtk_file_filter_add_pattern (filter, "*.cert");

	return filter;
}

static void
seahorse_certificate_der_exporter_init (SeahorseCertificateDerExporter *self)
{

}

static void
seahorse_certificate_der_exporter_get_property (GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec)
{
	SeahorseCertificateDerExporter *self = SEAHORSE_CERTIFICATE_DER_EXPORTER (object);

	switch (prop_id) {
	case PROP_FILENAME:
		g_value_take_string (value, calc_filename (self));
		break;
	case PROP_CONTENT_TYPE:
		g_value_set_string (value, calc_content_type (self));
		break;
	case PROP_FILE_FILTER:
		g_value_take_object (value, calc_file_filter (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_certificate_der_exporter_finalize (GObject *obj)
{
	SeahorseCertificateDerExporter *self = SEAHORSE_CERTIFICATE_DER_EXPORTER (obj);

	g_clear_object (&self->certificate);
	g_list_free (self->objects);

	G_OBJECT_CLASS (seahorse_certificate_der_exporter_parent_class)->finalize (obj);
}

static void
seahorse_certificate_der_exporter_class_init (SeahorseCertificateDerExporterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = seahorse_certificate_der_exporter_finalize;
	gobject_class->get_property = seahorse_certificate_der_exporter_get_property;

	g_object_class_override_property (gobject_class, PROP_FILENAME, "filename");
	g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");
	g_object_class_override_property (gobject_class, PROP_FILE_FILTER, "file-filter");
}

static GList *
seahorse_certificate_der_exporter_get_objects (SeahorseExporter *exporter)
{
	SeahorseCertificateDerExporter *self = SEAHORSE_CERTIFICATE_DER_EXPORTER (exporter);
	return self->objects;
}

static gboolean
seahorse_certificate_der_exporter_add_object (SeahorseExporter *exporter,
                                              GObject *object)
{
	SeahorseCertificateDerExporter *self = SEAHORSE_CERTIFICATE_DER_EXPORTER (exporter);

	if (self->certificate == NULL && SEAHORSE_IS_CERTIFICATE (object)) {
		self->certificate = g_object_ref (object);
		self->objects = g_list_append (self->objects, self->certificate);
		return TRUE;
	}

	return FALSE;
}

static void
seahorse_certificate_der_exporter_export_async (SeahorseExporter *exporter,
                                                GCancellable *cancellable,
                                                GAsyncReadyCallback callback,
                                                gpointer user_data)
{
	SeahorseCertificateDerExporter *self = SEAHORSE_CERTIFICATE_DER_EXPORTER (exporter);
	GSimpleAsyncResult *res;

	g_return_if_fail (self->certificate != NULL);

	res = g_simple_async_result_new (G_OBJECT (exporter), callback, user_data,
	                                 seahorse_certificate_der_exporter_export_async);

	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
}

static gpointer
seahorse_certificate_der_exporter_export_finish (SeahorseExporter *exporter,
                                                 GAsyncResult *result,
                                                 gsize *size,
                                                 GError **error)
{
	SeahorseCertificateDerExporter *self = SEAHORSE_CERTIFICATE_DER_EXPORTER (exporter);
	gconstpointer output;

	g_return_val_if_fail (self->certificate != NULL, NULL);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (exporter),
	                      seahorse_certificate_der_exporter_export_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	output = gcr_certificate_get_der_data (GCR_CERTIFICATE (self->certificate), size);
	return g_memdup (output, *size);
}

static void
seahorse_certificate_der_exporter_iface_init (SeahorseExporterIface *iface)
{
	iface->add_object = seahorse_certificate_der_exporter_add_object;
	iface->export_async = seahorse_certificate_der_exporter_export_async;
	iface->export_finish = seahorse_certificate_der_exporter_export_finish;
	iface->get_objects = seahorse_certificate_der_exporter_get_objects;
}

SeahorseExporter *
seahorse_certificate_der_exporter_new (SeahorseCertificate *certificate)
{
	SeahorseExporter *exporter;

	exporter = g_object_new (SEAHORSE_TYPE_CERTIFICATE_DER_EXPORTER,
	                         NULL);

	if (!seahorse_exporter_add_object (exporter, G_OBJECT (certificate)))
		g_assert_not_reached ();

	return exporter;
}
