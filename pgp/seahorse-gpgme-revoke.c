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

#include "config.h"

#include <string.h>
 
#include <glib/gi18n.h>
 
#include "seahorse-object-widget.h"
#include "seahorse-util.h"

#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-pgp-dialogs.h"

enum {
  COLUMN_TEXT,
  COLUMN_TOOLTIP,
  COLUMN_INT,
  N_COLUMNS
};


void               on_gpgme_revoke_ok_clicked               (GtkButton *button,
                                                             gpointer user_data);

G_MODULE_EXPORT void
on_gpgme_revoke_ok_clicked (GtkButton *button,
                            gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseRevokeReason reason;
	SeahorseGpgmeSubkey *subkey;
	const gchar *description;
	gpgme_error_t err;
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue value;
	
	widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "reason"));
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	
	memset (&value, 0, sizeof(value));
	gtk_tree_model_get_value (model, &iter, COLUMN_INT, &value);
	reason = g_value_get_int (&value);
	g_value_unset (&value);
	
	description = gtk_entry_get_text (GTK_ENTRY (seahorse_widget_get_widget (swidget, "description")));
	subkey = g_object_get_data (G_OBJECT (swidget), "subkey");
	g_return_if_fail (SEAHORSE_IS_GPGME_SUBKEY (subkey));
	
	err = seahorse_gpgme_key_op_revoke_subkey (subkey, reason, description);
	if (!GPG_IS_OK (err))
		seahorse_gpgme_handle_error (err, _("Couldn't revoke subkey"));
	seahorse_widget_destroy (swidget);
}

void
seahorse_gpgme_revoke_new (SeahorseGpgmeSubkey *subkey, GtkWindow *parent)
{
	SeahorseWidget *swidget;
	gchar *title;
	const gchar *label;
	GtkWidget *widget;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	
	g_return_if_fail (SEAHORSE_IS_GPGME_SUBKEY (subkey));
	
	swidget = seahorse_widget_new ("revoke", parent);
	g_return_if_fail (swidget != NULL);
	
	label = seahorse_pgp_subkey_get_description (SEAHORSE_PGP_SUBKEY (subkey));
	title = g_strdup_printf (_("Revoke: %s"), label);
	gtk_window_set_title (GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)), title);
	g_free (title);
	
	g_object_set_data (G_OBJECT (swidget), "subkey", subkey);

	/* Initialize List Store for the Combo Box */
	store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                    COLUMN_TEXT, _("No reason"),
	                    COLUMN_TOOLTIP, _("No reason for revoking key"),
	                    COLUMN_INT, REVOKE_NO_REASON,
	                    -1);
                        
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                    COLUMN_TEXT, _("Compromised"),
	                    COLUMN_TOOLTIP, _("Key has been compromised"),
	                    COLUMN_INT, REVOKE_COMPROMISED,
	                    -1);     
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                    COLUMN_TEXT, _("Superseded"),
	                    COLUMN_TOOLTIP, _("Key has been superseded"),
	                    COLUMN_INT, REVOKE_SUPERSEDED,
	                    -1);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                    COLUMN_TEXT, _("Not Used"),
	                    COLUMN_TOOLTIP, _("Key is no longer used"),
	                    COLUMN_INT, REVOKE_NOT_USED,
	                    -1);
                        
	/* Finish Setting Up Combo Box */
	widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "reason"));
    
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
    
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer,
	                                "text", COLUMN_TEXT,
	                                NULL);
}

void
seahorse_gpgme_add_revoker_new (SeahorseGpgmeKey *pkey, GtkWindow *parent)
{
	SeahorseGpgmeKey *revoker;
	GtkWidget *dialog;
	gint response;
	gpgme_error_t err;
	const gchar *userid1, *userid2;
	
	g_return_if_fail (pkey != NULL && SEAHORSE_IS_GPGME_KEY (pkey));

	revoker = SEAHORSE_GPGME_KEY (seahorse_signer_get (parent));
	if (revoker == NULL)
		return;
	
	userid1 = seahorse_object_get_label (SEAHORSE_OBJECT (revoker));
	userid2 = seahorse_object_get_label (SEAHORSE_OBJECT (pkey));

	dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
	                                 GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
	                                 _("You are about to add %s as a revoker for %s."
	                                   " This operation cannot be undone! Are you sure you want to continue?"),
	                                   userid1, userid2);
    
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	if (response != GTK_RESPONSE_YES)
		return;
	
	err = seahorse_gpgme_key_op_add_revoker (pkey, revoker);
	if (!GPG_IS_OK (err))
		seahorse_gpgme_handle_error (err, _("Couldn't add revoker"));
}
