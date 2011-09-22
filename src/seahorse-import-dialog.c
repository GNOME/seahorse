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

#include "seahorse-import-dialog.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

struct _SeahorseImportDialog {
	GtkDialog parent;
	GcrViewerWidget *viewer;
	GcrImportButton *import;
};

struct _SeahorseImportDialogClass {
	GtkDialogClass parent_class;
};

G_DEFINE_TYPE (SeahorseImportDialog, seahorse_import_dialog, GTK_TYPE_DIALOG);

static void
on_viewer_renderer_added (GcrViewerWidget *viewer,
                          GcrRenderer *renderer,
                          GcrParser *parser,
                          gpointer user_data)
{
	SeahorseImportDialog *self = SEAHORSE_IMPORT_DIALOG (user_data);
	gcr_import_button_add_parsed (self->import, parser);
}

static void
seahorse_import_dialog_init (SeahorseImportDialog *self)
{

}

static void
on_import_button_imported (GcrImportButton *button,
                           GcrImporter *importer,
                           GError *error,
                           gpointer user_data)
{
	SeahorseImportDialog *self = SEAHORSE_IMPORT_DIALOG (user_data);
	GcrRenderer *renderer;

	if (error == NULL) {
		gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);

	} else {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			renderer = gcr_failure_renderer_new (_("Import failed"), error);
			gcr_viewer_add_renderer (GCR_VIEWER (self->viewer), renderer);
			g_object_unref (renderer);
		}
	}
}

static void
seahorse_import_dialog_constructed (GObject *obj)
{
	SeahorseImportDialog *self = SEAHORSE_IMPORT_DIALOG (obj);
	GtkWidget *button;

	G_OBJECT_CLASS (seahorse_import_dialog_parent_class)->constructed (obj);

	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (self), GTK_WIDGET (button), GTK_RESPONSE_CANCEL);

	self->import = gcr_import_button_new (_("Import"));
	g_signal_connect_object (self->import, "imported",
	                         G_CALLBACK (on_import_button_imported),
	                         self, 0);
	gtk_widget_show (GTK_WIDGET (self->import));
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_action_area (GTK_DIALOG (self))),
	                    GTK_WIDGET (self->import), FALSE, TRUE, 0);

	self->viewer = gcr_viewer_widget_new ();
	g_signal_connect_object (self->viewer, "added",
	                         G_CALLBACK (on_viewer_renderer_added),
	                         self, 0);
	gtk_widget_show (GTK_WIDGET (self->viewer));

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))),
	                    GTK_WIDGET (self->viewer), TRUE, TRUE, 0);
}

static void
seahorse_import_dialog_class_init (SeahorseImportDialogClass *klass)
{
	GObjectClass *gobject_class= G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_import_dialog_constructed;

	g_type_class_add_private (klass, sizeof (SeahorseImportDialog));
}

GtkDialog *
seahorse_import_dialog_new (GtkWindow *parent)
{
	g_return_val_if_fail (GTK_WINDOW (parent), NULL);

	return g_object_new (SEAHORSE_TYPE_IMPORT_DIALOG,
	                     "transient-for", parent,
	                     NULL);
}

void
seahorse_import_dialog_add_uris (SeahorseImportDialog *self,
                                 const gchar **uris)
{
	GFile *file;
	guint i;

	g_return_if_fail (SEAHORSE_IS_IMPORT_DIALOG (self));
	g_return_if_fail (uris != NULL);

	for (i = 0; uris[i] != NULL; i++) {
		file = g_file_new_for_uri (uris[i]);
		gcr_viewer_widget_load_file (self->viewer, file);
		g_object_unref (file);
	}
}

void
seahorse_import_dialog_add_text (SeahorseImportDialog *self,
                                 const gchar *display_name,
                                 const gchar *text)
{
	g_return_if_fail (SEAHORSE_IS_IMPORT_DIALOG (self));
	g_return_if_fail (text != NULL);
	gcr_viewer_widget_load_data (self->viewer, display_name,
	                             text, strlen (text));
}
