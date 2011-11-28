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

#include "seahorse-ssh.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-exporter.h"
#include "seahorse-ssh-source.h"

#include "seahorse-exporter.h"
#include "seahorse-object.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

#define SEAHORSE_SSH_EXPORTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_EXPORTER, SeahorseSshExporterClass))
#define SEAHORSE_IS_SSH_EXPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_EXPORTER))
#define SEAHORSE_SSH_EXPORTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_EXPORTER, SeahorseSshExporterClass))

typedef struct _SeahorseSshExporterClass SeahorseSshExporterClass;

struct _SeahorseSshExporter {
	GObject parent;
	SeahorseSSHKey *key;
	GList *objects;
	gboolean secret;
};

struct _SeahorseSshExporterClass {
	GObjectClass parent_class;
};

enum {
	PROP_0,
	PROP_FILENAME,
	PROP_CONTENT_TYPE,
	PROP_FILE_FILTER,
	PROP_SECRET
};

static void   seahorse_ssh_exporter_iface_init    (SeahorseExporterIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseSshExporter, seahorse_ssh_exporter, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_EXPORTER, seahorse_ssh_exporter_iface_init);
);

static gchar *
calc_filename (SeahorseSshExporter *self)
{
	SeahorseSSHKeyData *data;
	const gchar *location = NULL;
	const gchar *basename = NULL;
	gchar *filename;

	g_return_val_if_fail (self->key, NULL);

	data = seahorse_ssh_key_get_data (self->key);
	if (data && !data->partial) {
		if (self->secret && data->privfile)
			location = data->privfile;
		else if (!self->secret && data->pubfile)
			location = data->pubfile;
		if (location != NULL)
			return g_path_get_basename (location);
	}

	basename = seahorse_object_get_nickname (SEAHORSE_OBJECT (self->key));
	if (basename == NULL)
		basename = _("Ssh Key");

	if (self->secret) {
		filename = g_strdup_printf ("id_%s", basename);
		g_strstrip (filename);
		g_strdelimit (filename, SEAHORSE_BAD_FILENAME_CHARS " ", '_');
		return filename;
	} else {
		filename = g_strdup_printf ("%s.pub", basename);
		g_strstrip (filename);
		g_strdelimit (filename, SEAHORSE_BAD_FILENAME_CHARS, '_');
		return filename;
	}
}

static const gchar *
calc_content_type (SeahorseSshExporter *self)
{
	if (self->secret)
		return "application/x-pem-key";
	else
		return "application/x-ssh-key";
}

static GtkFileFilter *
calc_file_filter (SeahorseSshExporter *self)
{
	GtkFileFilter *filter = gtk_file_filter_new ();

	if (self->secret) {
		gtk_file_filter_set_name (filter, _("Secret SSH keys"));
		gtk_file_filter_add_mime_type (filter, "application/x-pem-key");
		gtk_file_filter_add_pattern (filter, "id_*");
	} else {
		gtk_file_filter_set_name (filter, _("Public SSH keys"));
		gtk_file_filter_add_mime_type (filter, "application/x-ssh-key");
		gtk_file_filter_add_pattern (filter, "*.pub");
	}

	return filter;
}

static void
seahorse_ssh_exporter_init (SeahorseSshExporter *self)
{

}

static void
seahorse_ssh_exporter_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	SeahorseSshExporter *self = SEAHORSE_SSH_EXPORTER (object);

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
	case PROP_SECRET:
		g_value_set_boolean (value, self->secret);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_ssh_exporter_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	SeahorseSshExporter *self = SEAHORSE_SSH_EXPORTER (object);

	switch (prop_id) {
	case PROP_SECRET:
		self->secret = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_ssh_exporter_finalize (GObject *obj)
{
	SeahorseSshExporter *self = SEAHORSE_SSH_EXPORTER (obj);

	g_clear_object (&self->key);

	G_OBJECT_CLASS (seahorse_ssh_exporter_parent_class)->finalize (obj);
}

static void
seahorse_ssh_exporter_class_init (SeahorseSshExporterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = seahorse_ssh_exporter_finalize;
	gobject_class->set_property = seahorse_ssh_exporter_set_property;
	gobject_class->get_property = seahorse_ssh_exporter_get_property;

	g_object_class_override_property (gobject_class, PROP_FILENAME, "filename");
	g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");
	g_object_class_override_property (gobject_class, PROP_FILE_FILTER, "file-filter");

	g_object_class_install_property (gobject_class, PROP_SECRET,
	           g_param_spec_boolean ("secret", "Secret", "Secret key export",
	                                 FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static GList *
seahorse_ssh_exporter_get_objects (SeahorseExporter *exporter)
{
	SeahorseSshExporter *self = SEAHORSE_SSH_EXPORTER (exporter);
	return self->objects;
}

static gboolean
seahorse_ssh_exporter_add_object (SeahorseExporter *exporter,
                                  GObject *object)
{
	SeahorseSshExporter *self = SEAHORSE_SSH_EXPORTER (exporter);
	SeahorseSSHKey *key;
	SeahorseUsage usage;

	if (SEAHORSE_IS_SSH_KEY (object) && !self->key) {
		key = SEAHORSE_SSH_KEY (object);
		if (self->secret) {
			usage = seahorse_object_get_usage (SEAHORSE_OBJECT (object));
			if (usage != SEAHORSE_USAGE_PRIVATE_KEY)
				return FALSE;
		}
		self->key = g_object_ref (key);
		self->objects = g_list_append (self->objects, self->key);
		return TRUE;
	}

	return FALSE;
}

static void
seahorse_ssh_exporter_export_async (SeahorseExporter *exporter,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	SeahorseSshExporter *self = SEAHORSE_SSH_EXPORTER (exporter);
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_ssh_exporter_export_async);

	g_simple_async_result_complete_in_idle (res);
	g_object_unref (res);
 }

static gpointer
seahorse_ssh_exporter_export_finish (SeahorseExporter *exporter,
                                     GAsyncResult *result,
                                     gsize *size,
                                     GError **error)
{
	SeahorseSshExporter *self = SEAHORSE_SSH_EXPORTER (exporter);
	SeahorseSSHKeyData *keydata;
	gpointer results = NULL;
	SeahorsePlace *place;
	gsize n_results;

	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (exporter),
	                      seahorse_ssh_exporter_export_async), NULL);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return NULL;

	keydata = seahorse_ssh_key_get_data (self->key);

	if (self->secret) {
		place = seahorse_object_get_place (SEAHORSE_OBJECT (self->key));
		results = seahorse_ssh_source_export_private (SEAHORSE_SSH_SOURCE (place),
		                                              self->key, &n_results, error);
	} else {
		if (keydata->pubfile) {
			g_assert (keydata->rawdata);
			results = g_strdup_printf ("%s\n", keydata->rawdata);
			n_results = strlen (results);
		} else {
			g_set_error (error, SEAHORSE_ERROR, 0, "%s",
			             _("No public key file is available for this key."));
		}
	}

	*size = n_results;
	return results;
}

static void
seahorse_ssh_exporter_iface_init (SeahorseExporterIface *iface)
{
	iface->add_object = seahorse_ssh_exporter_add_object;
	iface->export_async = seahorse_ssh_exporter_export_async;
	iface->export_finish = seahorse_ssh_exporter_export_finish;
	iface->get_objects = seahorse_ssh_exporter_get_objects;
}

SeahorseExporter *
seahorse_ssh_exporter_new (GObject *object,
                           gboolean secret)
{
	SeahorseExporter *exporter;

	exporter = g_object_new (SEAHORSE_TYPE_SSH_EXPORTER,
	                         "secret", secret,
	                         NULL);

	if (!seahorse_exporter_add_object (exporter, object))
		g_assert_not_reached ();

	return exporter;
}
