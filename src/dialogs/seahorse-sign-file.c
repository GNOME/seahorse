/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <gnome.h>

#include "seahorse-file-dialogs.h"
#include "seahorse-widget.h"
#include "seahorse-ops-file.h"

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	const gchar *file;
	
	file = gtk_file_selection_get_filename (GTK_FILE_SELECTION (
		glade_xml_get_widget (swidget->xml, swidget->name)));
	
	if (file == NULL || g_str_equal (file, "")) {
		seahorse_util_show_error (NULL, _("You must choose an existing file"));
		return;
	}
	
	if (seahorse_sign_file (swidget->sctx, file))
		seahorse_widget_destroy (swidget);
}
	
void
seahorse_sign_file_new (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("sign-file", sctx);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
}

gboolean
seahorse_sign_file (SeahorseContext *sctx, const gchar *file)
{
	GtkWidget *dialog;
	
	if (seahorse_ops_file_sign (sctx, file)) {
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
			_("Signed file is %s"),
			seahorse_ops_file_add_suffix (file, SEAHORSE_SIG_FILE, sctx));
		g_signal_connect_swapped (GTK_DIALOG (dialog), "response",
			G_CALLBACK (gtk_widget_destroy), GTK_WIDGET (dialog));
		gtk_widget_show (dialog);
		return TRUE;
	}
	else {
		seahorse_util_show_error (NULL, g_strdup_printf (_("Could not sign %s"), file));
		return FALSE;
	}
}
