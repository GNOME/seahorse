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
	
	if (!seahorse_ops_file_check_suffix (file, SEAHORSE_SIG_FILE)) {
		seahorse_util_show_error (NULL,
			_("You must choose a signature file ending in .sig or .asc"));
		return;
	}
	
	if (seahorse_verify_file (swidget->sctx, file))
		seahorse_widget_destroy (swidget);
}

/**
 * seahorse_verify_file_new:
 * @sctx: #SeahorseContext
 *
 * Loads a file dialog for choosing a signature file to verify.
 **/
void
seahorse_verify_file_new (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("verify-file", sctx);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
}

/**
 * seahorse_verify_file:
 * @sctx: #SeahorseContext
 * @file: Filename of signature file to verify.
 * Should have a .sig or .asc extension.
 *
 * Tries to verify @file using seahorse_ops_file_verify(), then shows
 * the signature status dialog with seahorse_signatures_new().
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_verify_file (SeahorseContext *sctx, const gchar *file)
{
	GpgmeSigStat status;
	
	if (seahorse_ops_file_verify (sctx, file, &status)) {
		seahorse_signatures_new (sctx, status);
		return TRUE;
	}
	else {
		seahorse_util_show_error (NULL, g_strdup_printf (_("Could not verify %s"), file));
		return FALSE;
	}
}
