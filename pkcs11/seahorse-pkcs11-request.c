/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "seahorse-pkcs11-request.h"
#include "seahorse-token.h"

#include "seahorse-progress.h"
#include "seahorse-interaction.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

#include <gcr/gcr.h>

#include <gck/gck.h>

GType   seahorse_pkcs11_request_get_type           (void) G_GNUC_CONST;

#define SEAHORSE_TYPE_PKCS11_REQUEST               (seahorse_pkcs11_request_get_type ())
#define SEAHORSE_PKCS11_REQUEST(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_REQUEST, SeahorsePkcs11Request))
#define SEAHORSE_IS_PKCS11_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_REQUEST))
#define SEAHORSE_PKCS11_REQUEST_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_REQUEST, SeahorsePkcs11RequestClass))
#define SEAHORSE_IS_PKCS11_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_REQUEST))
#define SEAHORSE_PKCS11_REQUEST_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_REQUEST, SeahorsePkcs11RequestClass))

typedef struct _SeahorsePkcs11Request SeahorsePkcs11Request;
typedef struct _SeahorsePkcs11RequestClass SeahorsePkcs11RequestClass;

struct _SeahorsePkcs11Request {
	GtkDialog dialog;

	GtkEntry *name_entry;
	GckObject *private_key;

	guchar *encoded;
	gsize n_encoded;
};

struct _SeahorsePkcs11RequestClass {
	GtkDialogClass dialog_class;
};

enum {
	PROP_0,
	PROP_PRIVATE_KEY
};

G_DEFINE_TYPE (SeahorsePkcs11Request, seahorse_pkcs11_request, GTK_TYPE_DIALOG);

enum {
	MECHANISM_LABEL,
	MECHANISM_TYPE,
	MECHANISM_N_COLS
};

static void
seahorse_pkcs11_request_init (SeahorsePkcs11Request *self)
{

}

static void
update_response (SeahorsePkcs11Request *self)
{
	const gchar *name = gtk_entry_get_text (self->name_entry);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, !g_str_equal (name, ""));
}

static void
on_request_file_written (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (user_data);
	GError *error = NULL;
	GtkWindow *parent;

	parent = gtk_window_get_transient_for (GTK_WINDOW (self));
	if (!g_file_replace_contents_finish (G_FILE (source), result, NULL, &error))
		seahorse_util_handle_error (&error, parent, _("Couldn't save certificate request"));

	g_object_unref (self);
}

static const gchar *BAD_FILENAME_CHARS = "/\\<>|?*";

static void
save_certificate_request (SeahorsePkcs11Request *self,
                          GcrCertificateRequest *req,
                          GtkWindow *parent)
{
	GtkFileChooser *chooser;
	GtkFileFilter *pem_filter;
	GtkFileFilter *der_filter;
	GtkWidget *dialog;
	gchar *filename;
	gboolean textual;
	gchar *label;
	GFile *file;
	gint response;

	parent = gtk_window_get_transient_for (GTK_WINDOW (self));
	dialog = gtk_file_chooser_dialog_new (_("Save certificate request"),
	                                      parent, GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                      NULL);

	chooser = GTK_FILE_CHOOSER (dialog);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_local_only (chooser, FALSE);

	der_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (der_filter, _("Certificate request"));
	gtk_file_filter_add_mime_type (der_filter, "application/pkcs10");
	gtk_file_filter_add_pattern (der_filter, "*.p10");
	gtk_file_filter_add_pattern (der_filter, "*.csr");
	gtk_file_chooser_add_filter (chooser, der_filter);
	gtk_file_chooser_set_filter (chooser, der_filter);

	pem_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (pem_filter, _("PEM encoded request"));
	gtk_file_filter_add_mime_type (pem_filter, "application/pkcs10+pem");
	gtk_file_filter_add_pattern (pem_filter, "*.pem");
	gtk_file_chooser_add_filter (chooser, pem_filter);

	label = NULL;
	g_object_get (self->private_key, "label", &label, NULL);
	if (!label || g_str_equal (label, ""))
		label = g_strdup ("Certificate Request");
	filename = g_strconcat (label, ".csr", NULL);
	g_strdelimit (filename, BAD_FILENAME_CHARS, '_');
	gtk_file_chooser_set_current_name (chooser, filename);
	g_free (label);
	g_free (filename);

	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT) {

		g_free (self->encoded);
		textual = gtk_file_chooser_get_filter (chooser) == pem_filter;
		self->encoded = gcr_certificate_request_encode (req, textual, &self->n_encoded);
		g_return_if_fail (self->encoded != NULL);

		file = gtk_file_chooser_get_file (chooser);
		g_file_replace_contents_async (file, (const gchar *)self->encoded,
		                               self->n_encoded, NULL, FALSE,
		                               G_FILE_CREATE_NONE, NULL,
		                               on_request_file_written, g_object_ref (self));
		g_object_unref (file);
	}

	gtk_widget_destroy (dialog);
}

static void
complete_request (SeahorsePkcs11Request *self,
                  GcrCertificateRequest *req,
                  GError **error)
{
	GtkWindow *parent;

	g_assert (error != NULL);

	parent = gtk_window_get_transient_for (GTK_WINDOW (self));
	if (*error != NULL)
		seahorse_util_handle_error (error, parent, _("Couldn't create certificate request"));
	else
		save_certificate_request (self, req, parent);
}

static void
on_request_complete (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (user_data);
	GcrCertificateRequest *req = GCR_CERTIFICATE_REQUEST (source);
	GError *error = NULL;

	gcr_certificate_request_complete_finish (req, result, &error);
	complete_request (self, req, &error);

	g_object_unref (self);
}

static void
on_name_entry_changed (GtkEditable *editable,
                       gpointer user_data)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (user_data);
	update_response (self);
}

static void
seahorse_pkcs11_request_constructed (GObject *obj)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (obj);
	GtkBuilder *builder;
	const gchar *path;
	GError *error = NULL;
	GtkWidget *content;
	GtkWidget *widget;

	G_OBJECT_CLASS (seahorse_pkcs11_request_parent_class)->constructed (obj);

	builder = gtk_builder_new ();
	path = SEAHORSE_UIDIR "seahorse-pkcs11-request.xml";
	gtk_builder_add_from_file (builder, path, &error);
	if (error != NULL) {
		g_warning ("couldn't load ui file: %s", path);
		g_clear_error (&error);
		g_object_unref (builder);
		return;
	}

	gtk_window_set_resizable (GTK_WINDOW (self), FALSE);
	content = gtk_dialog_get_content_area (GTK_DIALOG (self));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "pkcs11-request"));
	gtk_container_add (GTK_CONTAINER (content), widget);
	gtk_widget_show (widget);

	self->name_entry = GTK_ENTRY (gtk_builder_get_object (builder, "request-name"));
	g_signal_connect (self->name_entry, "changed", G_CALLBACK (on_name_entry_changed), self);

	/* The buttons */
	gtk_dialog_add_buttons (GTK_DIALOG (self),
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                        _("Create"), GTK_RESPONSE_OK,
	                        NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);

	update_response (self);
	g_object_unref (builder);

	g_return_if_fail (GCK_IS_OBJECT (self->private_key));
}

static void
seahorse_pkcs11_request_set_property (GObject *obj,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (obj);

	switch (prop_id) {
	case PROP_PRIVATE_KEY:
		g_return_if_fail (self->private_key == NULL);
		self->private_key = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_request_get_property (GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (obj);

	switch (prop_id) {
	case PROP_PRIVATE_KEY:
		g_value_set_object (value, self->private_key);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_request_finalize (GObject *obj)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (obj);

	g_clear_object (&self->private_key);
	g_free (self->encoded);

	G_OBJECT_CLASS (seahorse_pkcs11_request_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_request_response (GtkDialog *dialog,
                                  gint response_id)
{
	SeahorsePkcs11Request *self = SEAHORSE_PKCS11_REQUEST (dialog);
	GcrCertificateRequest *req;
	GTlsInteraction *interaction;
	GckSession *session;
	GtkWindow *parent;

	if (response_id == GTK_RESPONSE_OK) {
		g_return_if_fail (self->private_key != NULL);

		parent = gtk_window_get_transient_for (GTK_WINDOW (self));
		interaction = seahorse_interaction_new (parent);
		session = gck_object_get_session (self->private_key);
		gck_session_set_interaction (session, interaction);
		g_object_unref (interaction);
		g_object_unref (session);

		req = gcr_certificate_request_prepare (GCR_CERTIFICATE_REQUEST_PKCS10,
		                                       self->private_key);
		gcr_certificate_request_set_cn (req, gtk_entry_get_text (self->name_entry));
		gcr_certificate_request_complete_async (req, NULL,
		                                        on_request_complete, g_object_ref (self));
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
seahorse_pkcs11_request_class_init (SeahorsePkcs11RequestClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	gobject_class->get_property = seahorse_pkcs11_request_get_property;
	gobject_class->set_property = seahorse_pkcs11_request_set_property;
	gobject_class->constructed = seahorse_pkcs11_request_constructed;
	gobject_class->finalize = seahorse_pkcs11_request_finalize;

	dialog_class->response = seahorse_pkcs11_request_response;

	g_object_class_install_property (gobject_class, PROP_PRIVATE_KEY,
	            g_param_spec_object ("private-key", "Private key", "Private key",
	                                 GCK_TYPE_OBJECT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

void
seahorse_pkcs11_request_prompt (GtkWindow *parent,
                                GckObject *private_key)
{
	GtkDialog *dialog;

	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

	dialog = g_object_new (SEAHORSE_TYPE_PKCS11_REQUEST,
	                       "transient-for", parent,
	                       "private-key", private_key,
	                       NULL);
	g_object_ref_sink (dialog);

	gtk_dialog_run (dialog);

	g_object_unref (dialog);
}
