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
 
#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-util.h"

#include "pgp/seahorse-pgp-dialogs.h"
#include "pgp/seahorse-pgp-key-op.h"

#define EXPIRES "expires"
#define LENGTH "length"

enum {
  COMBO_STRING,
  COMBO_INT,
  N_COLUMNS
};

static void
type_changed (GtkComboBox *combo, SeahorseWidget *swidget)
{
	gint type;
	GtkSpinButton *length;
    GtkTreeModel *model;
    GtkTreeIter iter;
    	
	length = GTK_SPIN_BUTTON (glade_xml_get_widget (swidget->xml, LENGTH));
	
	model = gtk_combo_box_get_model (combo);
	gtk_combo_box_get_active_iter (combo, &iter);
	gtk_tree_model_get (model, &iter,
                        COMBO_INT, &type,
                        -1);
	
	switch (type) {
		/* DSA */
		case 0:
			gtk_spin_button_set_range (length, DSA_MIN, DSA_MAX);
			break;
		/* ElGamal */
		case 1:
			gtk_spin_button_set_range (length, ELGAMAL_MIN, LENGTH_MAX);
			break;
		/* RSA */
		default:
			gtk_spin_button_set_range (length, RSA_MIN, LENGTH_MAX);
			break;
	}
}

static void
never_expires_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, EXPIRES),
		!gtk_toggle_button_get_active (togglebutton));
}

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	SeahorseKeyEncType real_type;
	gint type;
	guint length;
	time_t expires;
	gpgme_error_t err;
	GtkWidget *widget;
	GtkComboBox *combo;
	GtkTreeModel *model;
    GtkTreeIter iter;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	combo = GTK_COMBO_BOX (glade_xml_get_widget (swidget->xml, "type"));
	gtk_combo_box_get_active_iter (combo, &iter);
	model = gtk_combo_box_get_model (combo);
	gtk_tree_model_get (model, &iter,
                        COMBO_INT, &type,
                        -1);	
		
	length = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (
		glade_xml_get_widget (swidget->xml, LENGTH)));
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	glade_xml_get_widget (swidget->xml, "never_expires"))))
		expires = 0;
	else
		expires = gnome_date_edit_get_time (GNOME_DATE_EDIT (
			glade_xml_get_widget (swidget->xml, EXPIRES)));
	
	switch (type) {
		case 0:
			real_type = DSA;
			break;
		case 1:
			real_type = ELGAMAL;
			break;
		case 2:
			real_type = RSA_SIGN;
			break;
		default:
			real_type = RSA_ENCRYPT;
			break;
	}
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_widget_set_sensitive (widget, FALSE);
	err = seahorse_pgp_key_pair_op_add_subkey (SEAHORSE_PGP_KEY (skwidget->skey), 
                                           real_type, length, expires);
	gtk_widget_set_sensitive (widget, TRUE);
	
	if (!GPG_IS_OK (err))
		seahorse_pgp_handle_gpgme_error (err, _("Couldn't add subkey"));

	seahorse_widget_destroy (swidget);
}

void
seahorse_add_subkey_new (SeahorsePGPKey *pkey, GtkWindow *parent)
{
	SeahorseWidget *swidget;
    gchar *userid;
    GtkComboBox* combo;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkCellRenderer *renderer;
	
	swidget = seahorse_key_widget_new ("add-subkey", parent, SEAHORSE_KEY (pkey));
	g_return_if_fail (swidget != NULL);
	
    userid = seahorse_key_get_name (SEAHORSE_KEY (pkey), 0);
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
		g_strdup_printf (_("Add subkey to %s"), userid));
    g_free (userid);
    
    combo = GTK_COMBO_BOX (glade_xml_get_widget (swidget->xml, "type"));
    model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT));
    
    gtk_combo_box_set_model (combo, model);
        
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));
    renderer = gtk_cell_renderer_text_new ();
    
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer,
                                    "text", COMBO_STRING);
                                    
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        COMBO_STRING, _("DSA (sign only)"),
                        COMBO_INT, 0,
                        -1);
                        
    gtk_combo_box_set_active_iter (combo, &iter);
    
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        COMBO_STRING, _("ElGamal (encrypt only)"),
                        COMBO_INT, 1,
                        -1);
                        
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        COMBO_STRING, _("RSA (sign only)"),
                        COMBO_INT, 2,
                        -1);
    
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        COMBO_STRING, _("RSA (encrypt only)"),
                        COMBO_INT, 3,
                        -1);
    
    glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "never_expires_toggled",
		G_CALLBACK (never_expires_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "type_changed",
		G_CALLBACK (type_changed), swidget);
}
