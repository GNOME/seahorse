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

#include <config.h> 
#include <gnome.h>
 
#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-libdialogs.h"
#include "seahorse-util.h"

#include "pgp/seahorse-gpgmex.h"
#include "pgp/seahorse-pgp-key-op.h"

enum {
  COLUMN_TEXT,
  COLUMN_TOOLTIP,
  COLUMN_INT,
  N_COLUMNS
};

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	guint index;
	SeahorseRevokeReason reason;
	const gchar *description;
	gpgme_error_t err;
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GValue value;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	index = skwidget->index;
	
	widget = glade_xml_get_widget (swidget->xml, "reason");
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
	
    memset(&value, 0, sizeof(value));
	gtk_tree_model_get_value (model, &iter, COLUMN_INT, &value);
    reason = g_value_get_int (&value);
    g_value_unset (&value);
	
	description = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, "description")));
	
	if (skwidget->index != 0) {
		err = seahorse_pgp_key_op_revoke_subkey (SEAHORSE_PGP_KEY (skwidget->skey), 
                                             skwidget->index, reason, description);
		if (!GPG_IS_OK (err))
			seahorse_util_handle_gpgme (err, _("Couldn't revoke subkey"));
	}
	seahorse_widget_destroy (swidget);
}

void
seahorse_revoke_new (SeahorsePGPKey *pkey, GtkWindow *parent, guint index)
{
	SeahorseWidget *swidget;
	gchar *title;
    gchar *userid;
    GtkWidget *widget;
    GtkListStore *store;
    GtkTreeIter iter;
	GtkCellRenderer *renderer;
	
	g_return_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey));
	g_return_if_fail (index <= seahorse_pgp_key_get_num_subkeys (pkey));
	
	swidget = seahorse_key_widget_new_with_index ("revoke", parent, SEAHORSE_KEY (pkey), index);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	
    userid = seahorse_key_get_name (SEAHORSE_KEY (pkey), 0);
	if (index)
		title = g_strdup_printf (_("Revoke Subkey %d of %s"), index, userid);
	else
		title = g_strdup_printf (_("Revoke %s"), userid);
	g_free (userid);
   
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml,
		                  swidget->name)), title);

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
    
    widget = glade_xml_get_widget (swidget->xml, "reason");
    
    gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (store));
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
    
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (widget), renderer,
                                    "text", COLUMN_TEXT,
                                    NULL);
}

void
seahorse_add_revoker_new (SeahorsePGPKey *pkey, GtkWindow *parent)
{
	SeahorsePGPKey *revoker;
	GtkWidget *dialog;
	gint response;
	gpgme_error_t err;
    gchar *userid1, *userid2;
	
	g_return_if_fail (pkey != NULL && SEAHORSE_IS_PGP_KEY (pkey));

    /* TODO: Limit to selecting only PGP keys */
    revoker = SEAHORSE_PGP_KEY (seahorse_signer_get (parent));
    if (revoker == NULL)
        return;
	
    userid1 = seahorse_key_get_name (SEAHORSE_KEY (revoker), 0);
    userid2 = seahorse_key_get_name (SEAHORSE_KEY (pkey), 0);

	dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
		GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
		_("You are about to add %s as a revoker for %s."
		" This operation cannot be undone! Are you sure you want to continue?"),
		userid1, userid2);
	
    g_free (userid1);
    g_free (userid2);
    
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	if (response != GTK_RESPONSE_YES)
		return;
	
	err = seahorse_pgp_key_pair_op_add_revoker (pkey, revoker);
	if (!GPG_IS_OK (err))
		seahorse_util_handle_gpgme (err, _("Couldn't add revoker"));
}
