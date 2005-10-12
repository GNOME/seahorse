/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Nate Nielsen
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
#include <time.h>

#include "seahorse-gpgmex.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-op.h"
#include "seahorse-util.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-key-op.h"
#include "seahorse-gtkstock.h"

#define NOTEBOOK "notebook"
#define SPACING 12

/* owner uid list */
enum {
	UID_INDEX,
	UID_NAME,
	UID_EMAIL,
	UID_COMMENT,
	UID_N_COLUMNS
};

/* details subkey list */
enum {
	SUBKEY_INDEX,
	SUBKEY_ID,
	SUBKEY_TYPE,
	SUBKEY_CREATED,
	SUBKEY_EXPIRES,
	SUBKEY_STATUS,
	SUBKEY_LENGTH,
	SUBKEY_N_COLUMNS
};

/* signatures */
enum {
	SIGN_KEY_ID,
	SIGN_NAME,
	SIGN_EMAIL,
	SIGN_COMMENT,
	SIGN_N_COLUMNS
};

static gint
get_selected_uid (SeahorseWidget *swidget)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *rows;
	gint index = -1;

	widget = glade_xml_get_widget (swidget->xml, "owner-userid-tree");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_assert (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE);
	
	rows = gtk_tree_selection_get_selected_rows (selection, NULL);

	if (g_list_length (rows) > 0) {
		gtk_tree_model_get_iter (model, &iter, rows->data);
		gtk_tree_model_get (model, &iter, UID_INDEX, &index, -1);
	}

	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (rows);

	return index;
}

static void
owner_add_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	seahorse_add_uid_new (SEAHORSE_PGP_KEY (skey));
}

static void 
owner_primary_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gpgme_error_t err;
	gint index;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	index = get_selected_uid (swidget);
	
	if (0 < index) {
		err = seahorse_pgp_key_op_primary_uid (SEAHORSE_PGP_KEY (skey), index);
		if (!GPG_IS_OK (err)) 
			seahorse_util_handle_gpgme (err, _("Couldn't change primary user ID"));
	} else if (index == 0) {}
}

static void 
owner_sign_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint index;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	index = get_selected_uid (swidget);
	if (index >= 1) 
		seahorse_sign_uid_show (SEAHORSE_PGP_KEY (skey), index);
}

static void 
owner_delete_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint index;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	index = get_selected_uid (swidget);
	if (index >= 1) 
		seahorse_delete_userid_show (SEAHORSE_PGP_KEY (skey), index);
}

static void
redraw_key_properties_dialog (GtkExpander *expander, SeahorseWidget *swidget)
{
	GtkExpander *expander1;
	GtkExpander *expander2;
	gboolean expanded;
	
	expander1 = GTK_EXPANDER (glade_xml_get_widget (swidget->xml, "owner-expander"));
	expander2 = GTK_EXPANDER (glade_xml_get_widget (swidget->xml, "subkey-expander"));
	
	expanded = gtk_expander_get_expanded (expander);
	
	if (expander == expander1) {
		gtk_expander_set_expanded (GTK_EXPANDER (expander2), !expanded);
	} else if (expander == expander2) {
		gtk_expander_set_expanded (GTK_EXPANDER (expander1), !expanded);
	}	
}

/*
 * Begin Photo ID Functions
 */
static void
owner_photo_add_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
    SeahorsePGPKey *pkey;
	char* filename = NULL;
    gpgme_error_t gerr;
	GtkWidget *chooser;
	
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY(skey);
    
    chooser = seahorse_util_chooser_open_new (_("Add Photo ID"), 
                	GTK_WINDOW(glade_xml_get_widget (swidget->xml, "key-properties")));             	
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);		
    seahorse_util_chooser_show_jpeg_files (chooser);
	
	if(gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
        
    gtk_widget_destroy (chooser);

    g_return_if_fail(filename);

    gerr = seahorse_pgp_key_op_photoid_add(pkey, filename);
    if (!GPG_IS_OK (gerr)) {
        
        /* A special error value set by seahorse_key_op_photoid_add to 
           denote an invalid format file */
        if (gerr == GPG_E (GPG_ERR_USER_1)) 
            /* TODO: We really shouldn't be getting here. We should rerender 
               the photo into a format that will work (along with resizing). */
            seahorse_util_show_error (NULL, _("Couldn't add photo"), 
                                      _("The file could not be loaded. It may be in an invalid format"));
        else        
            seahorse_util_handle_gpgme (gerr, _("Couldn't add photo"));        
    }
    
    g_free (filename);
    g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
}
 
static void
owner_photo_delete_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	gpgme_error_t gerr;
	GtkWidget *question, *delete_button, *cancel_button;
    gint response, uid; 
    gpgmex_photo_id_t photoid;

    photoid	= (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");
    
	g_return_if_fail(photoid != NULL);
    
    question = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                        _("Are you sure you want to permanently delete the current photo ID?"));
    
    delete_button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
    cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

    /* add widgets to action area */
    gtk_dialog_add_action_widget (GTK_DIALOG (question), GTK_WIDGET (cancel_button), GTK_RESPONSE_REJECT);
    gtk_dialog_add_action_widget (GTK_DIALOG (question), GTK_WIDGET (delete_button), GTK_RESPONSE_ACCEPT);
   
    /* show widgets */
    gtk_widget_show (delete_button);
    gtk_widget_show (cancel_button);
       
    response = gtk_dialog_run (GTK_DIALOG (question));
    gtk_widget_destroy (question);
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY(skey);
    
    uid = photoid->uid;
    
    g_return_if_fail(response == GTK_RESPONSE_ACCEPT);
        
    gerr = seahorse_pgp_key_op_photoid_delete (pkey, uid);
    if (!GPG_IS_OK (gerr))
        seahorse_util_handle_gpgme (gerr, _("Couldn't delete photo"));
    
    g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
}

static void
owner_photo_primary_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{

}

static void
set_photoid_state(SeahorseWidget *swidget, SeahorsePGPKey *pkey)
{
    SeahorseKeyEType etype; 
    GtkWidget *next_button; 
	GtkWidget *prev_button; 
	GtkWidget *del_button; 
	GtkWidget *add_button;
	GtkWidget *photo_image;
	
	gpgmex_photo_id_t photoid;
	
	etype = seahorse_key_get_etype (SEAHORSE_KEY(pkey));
	next_button = glade_xml_get_widget(swidget->xml, "owner-photo-next-button");
	prev_button = glade_xml_get_widget(swidget->xml, "owner-photo-previous-button");
	del_button = glade_xml_get_widget(swidget->xml, "owner-photo-delete-button");
	add_button = glade_xml_get_widget(swidget->xml, "owner-photo-add-button");
	photo_image = glade_xml_get_widget (swidget->xml, "photoid");
	
	photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");
	
	if (etype == SKEY_PRIVATE) {
	    gtk_widget_set_sensitive(add_button, TRUE);
	    
	    if (photoid != NULL)
	       gtk_widget_set_sensitive(del_button, TRUE);
	       
	} else {
	   gtk_widget_set_sensitive (add_button, FALSE);
	   gtk_widget_set_sensitive (del_button, FALSE);
	}

    if (photoid != NULL) {	
    	if (photoid == pkey->photoids)
    	   gtk_widget_set_sensitive (prev_button, FALSE);
    	else
    	   gtk_widget_set_sensitive (prev_button, TRUE);
    	   
    	if ((photoid->next != NULL) && (photoid->next->photo != NULL))
            gtk_widget_set_sensitive (next_button, TRUE);
        else
            gtk_widget_set_sensitive (next_button, FALSE);
    } else {
        gtk_widget_set_sensitive (del_button, FALSE);
        gtk_widget_set_sensitive (prev_button, FALSE);
        gtk_widget_set_sensitive (next_button, FALSE);
    }
        
    if ((photoid == NULL) || (photoid->photo == NULL))
        gtk_image_set_from_stock (GTK_IMAGE (photo_image), 
                                  SEAHORSE_STOCK_PERSON, 
                                  (GtkIconSize)-1);
    else 
        gtk_image_set_from_pixbuf (GTK_IMAGE (photo_image), photoid->photo);
}

static void
do_photo_id (SeahorseWidget *swidget)
{
	SeahorseKey *skey;
    SeahorsePGPKey *pkey;

	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    g_object_set_data (G_OBJECT (swidget), "current-photoid", pkey->photoids);
    
    set_photoid_state (swidget, pkey);
}	

static void
photoid_next_clicked(GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	gpgmex_photo_id_t photoid;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);
	
	photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget),
"current-photoid");
	
	if ((photoid != NULL) && (photoid->next != NULL))
        g_object_set_data (G_OBJECT (swidget), "current-photoid", photoid->next);
        
    set_photoid_state (swidget, pkey);
}

static void
photoid_prev_clicked(GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	gpgmex_photo_id_t itter, photoid;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);
	
	photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");
	
	if (photoid != pkey->photoids) {
    	itter = pkey->photoids;
    	
    	while ((itter->next != photoid) && (itter->next != NULL))
    	   itter = itter->next;
    	
    	g_object_set_data (G_OBJECT (swidget), "current-photoid", itter);
	}
	
	set_photoid_state (swidget, pkey);
}

static void
do_owner_signals (SeahorseWidget *swidget)
{ 
	SeahorseKey *skey;
	SeahorseKeyEType etype;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	etype = seahorse_key_get_etype (skey);
	
	glade_xml_signal_connect_data (swidget->xml, "on_owner_sign_button_clicked",
								G_CALLBACK (owner_sign_button_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_owner_expander_activate",
								G_CALLBACK (redraw_key_properties_dialog), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_subkey_expander_activate",
								G_CALLBACK (redraw_key_properties_dialog), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_next_clicked",
								G_CALLBACK (photoid_next_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_previous_clicked",
								G_CALLBACK (photoid_prev_clicked), swidget);
	
	if (etype == SKEY_PRIVATE ) {
		glade_xml_signal_connect_data (swidget->xml, "on_owner_add_button_clicked",
			G_CALLBACK (owner_add_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_owner_primary_button_clicked",
			G_CALLBACK (owner_primary_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_owner_delete_button_clicked",
			G_CALLBACK (owner_delete_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_primary_button_clicked",
			G_CALLBACK (owner_photo_primary_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_add_button_clicked",
			G_CALLBACK (owner_photo_add_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_delete_button_clicked",
			G_CALLBACK (owner_photo_delete_button_clicked), swidget);
	} else {
		
		gtk_widget_hide 
			(glade_xml_get_widget (swidget->xml, "owner-add-button"));
		gtk_widget_hide 
			(glade_xml_get_widget (swidget->xml, "owner-delete-button"));
		gtk_widget_hide 
			(glade_xml_get_widget (swidget->xml, "owner-primary-button"));
		gtk_widget_hide 
			(glade_xml_get_widget (swidget->xml, "owner-photo-add-button"));
		gtk_widget_hide 
			(glade_xml_get_widget (swidget->xml, "owner-photo-delete-button"));
		gtk_widget_hide 
			(glade_xml_get_widget (swidget->xml, "owner-photo-primary-button"));
	}
}

static void	
do_owner (SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	SeahorseValidity userid_validity;

	gpgme_user_id_t uid;
	
	GtkWidget *widget;
	GtkListStore *store;
	GtkTreeIter iter;
	gchar *text;
	const gchar *name, *email, *comment;
	guint8 i;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);
	
	if ((text = seahorse_pgp_key_get_userid_name (pkey, 0)) != NULL) {
		widget = glade_xml_get_widget (swidget->xml, "owner-name-label");
		gtk_label_set_text (GTK_LABEL (widget), text);  
		
		widget = glade_xml_get_widget (swidget->xml, swidget->name);
		gtk_window_set_title (GTK_WINDOW (widget), text);
		g_free (text);
	}
	
	if ((text = seahorse_pgp_key_get_userid_email (pkey, 0)) != NULL) {
		widget = glade_xml_get_widget (swidget->xml, "owner-email-label");
		gtk_label_set_text (GTK_LABEL (widget), text);  
		g_free (text);
	}
	
	if ((text = seahorse_pgp_key_get_userid_comment (pkey, 0)) != NULL) {
		widget = glade_xml_get_widget (swidget->xml, "owner-comment-label");
		gtk_label_set_text (GTK_LABEL (widget), text);  
		g_free (text);
	}
	
	/* temporarly disable these 2 buttons until the features get implemented */
	gtk_widget_set_sensitive
		(glade_xml_get_widget (swidget->xml, "owner-photo-first-button"), FALSE);
	gtk_widget_set_sensitive
		(glade_xml_get_widget (swidget->xml, "owner-photo-primary-button"), FALSE);
			
	/* Clear/create table store */
	widget = glade_xml_get_widget (swidget->xml, "owner-userid-tree");
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));	
	
	if (!store) {
		 
		/* This is our first time so create a store */
		store = gtk_list_store_new (UID_N_COLUMNS,
									G_TYPE_INT,   /* index */
									G_TYPE_STRING, 	/* name */
									G_TYPE_STRING, 	/* email */
									G_TYPE_STRING 	/* comment */
												  ); 
																		 
		/* Make the columns for the view */
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
					 -1,
					 "Name",
					 gtk_cell_renderer_text_new (), 
					 "text", UID_NAME, 
					 NULL);
		
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
					-1,
					"Email",
					gtk_cell_renderer_text_new (), 
					"text", UID_EMAIL, 
					NULL);
		
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
					-1,
					"Comment",
					gtk_cell_renderer_text_new (), 
					"text", UID_COMMENT, 
					NULL);
	} else {
		gtk_list_store_clear (GTK_LIST_STORE (store));
	}
	
	uid = pkey->pubkey->uids;
	for (i = 1; uid; uid = uid->next, i++) {
		/*why do I need to minus the index by 1 here? It seems like there is a contradiction between
		 the starting index from seahorse_pgp_key_get_userid_name & seahorse_pgp_key_op_primary
		 I could either i-1 here or i+1 on the UID_INDEX_COLUMN value!!!!!*/
		name = seahorse_pgp_key_get_userid_name (pkey, i-1);
		email = seahorse_pgp_key_get_userid_email (pkey, i-1);
		comment = seahorse_pgp_key_get_userid_comment (pkey, i-1);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,  
										UID_INDEX, i,
										UID_NAME, name,
										UID_EMAIL, email,
										UID_COMMENT, comment,
										-1);
	} 
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(store));
	do_photo_id (swidget);
}

static gint
get_selected_subkey (SeahorseWidget *swidget)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GList *rows;
	gint index = -1;
	
	
	widget = glade_xml_get_widget (swidget->xml, "details-subkey-tree");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_assert (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE);
	
	rows = gtk_tree_selection_get_selected_rows (selection, NULL);

	if (g_list_length (rows) > 0) {
		gtk_tree_model_get_iter (model, &iter, rows->data);
		gtk_tree_model_get (model, &iter, SUBKEY_INDEX, &index, -1);
	}

	g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
	g_list_free (rows);      
	
	return index;
}

static void
details_add_subkey_button_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);
	
	seahorse_add_subkey_new (pkey);
}
	
static void
details_del_subkey_button_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKey *skey = SEAHORSE_KEY_WIDGET (swidget)->skey;

	seahorse_delete_subkey_new (SEAHORSE_PGP_KEY (skey), get_selected_subkey (swidget));
}

static void
details_revoke_subkey_button_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKey *skey = SEAHORSE_KEY_WIDGET (swidget)->skey;

	seahorse_revoke_new (SEAHORSE_PGP_KEY (skey), get_selected_subkey (swidget));
}

static void
details_date_subkey_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	if (seahorse_key_get_etype (skey) == SKEY_PRIVATE)
		seahorse_expires_new (SEAHORSE_PGP_KEY (skey), get_selected_subkey (swidget));
}

static void
trust_changed (GtkComboBox *selection, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	gint trust;
	
	gpgme_error_t err;
	
	trust = gtk_combo_box_get_active (selection) + 1;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	if (seahorse_key_get_trust (skey) != trust) {
		err = seahorse_pgp_key_op_set_trust (SEAHORSE_PGP_KEY (skey), trust);
		if (err) {
			seahorse_util_handle_gpgme (err, "Unable to change trust");
		}
	}
}

/*
 * Makes better sense to present user with enable trust option.
 */
static void
trust_enable_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	SeahorseKey *skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	GtkWidget *combobox;
	gboolean trust_flag;
	gint validity;
		
	validity = seahorse_key_get_validity (skey);
	
	trust_flag = gtk_toggle_button_get_active (togglebutton);
	seahorse_pgp_key_op_set_disabled (SEAHORSE_PGP_KEY (skey), !trust_flag);
	combobox = glade_xml_get_widget (swidget->xml, "details-trust-combobox");
	gtk_widget_set_sensitive (combobox, trust_flag);
	
	/* make sure that the validity is valid before setting the combobox value 
		refer to seahorse-validity.h SeahorseValidity */
	if ( 0 < validity ) 
		gtk_combo_box_set_active (GTK_COMBO_BOX (combobox),  validity-1);
}

static void
details_passphrase_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	if (seahorse_key_get_etype (skey) == SKEY_PRIVATE)
		seahorse_pgp_key_pair_op_change_pass (SEAHORSE_PGP_KEY (skey));
}

static void
details_export_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	GtkWidget *dialog;
	gchar* uri = NULL;
	GError *err = NULL;
	GList *keys = NULL;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	keys = g_list_prepend (keys, skey);
	
	dialog = seahorse_util_chooser_save_new (_("Export Complete Key"), 
				GTK_WINDOW(seahorse_widget_get_top (swidget)));
	seahorse_util_chooser_show_key_files (dialog);
	seahorse_util_chooser_set_filename (dialog, keys);
	
	uri = seahorse_util_chooser_save_prompt (dialog);
	if(uri) {
		seahorse_op_export_file (keys, TRUE, uri, &err);

		if (err != NULL) {
			seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
										seahorse_util_uri_get_last (uri));
		}
		g_free (uri);
	}
	g_list_free (keys);
}

static void
do_details_signals (SeahorseWidget *swidget) 
{ 
	SeahorseKey *skey;
	SeahorseKeyEType etype;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	etype = seahorse_key_get_etype (skey);
	
	/* if not the key owner, disable most everything
		if key owner, add the callbacks to the subkey buttons	*/
	 if (etype == SKEY_PUBLIC ) {
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "details-actions-label"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "details-passphrase-button"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "details-export-button"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "details-add-button"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "details-date-button"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "details-revoke-button"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "details-delete-button"));
	} else {

		glade_xml_signal_connect_data (swidget->xml, "on_details_passphrase_button_clicked",
										G_CALLBACK (details_passphrase_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_details_add_button_clicked", 
										G_CALLBACK (details_add_subkey_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_details_date_button_clicked",
										G_CALLBACK (details_date_subkey_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_details_revoke_button_clicked",
										G_CALLBACK (details_revoke_subkey_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_details_delete_button_clicked",
										G_CALLBACK (details_del_subkey_button_clicked), swidget);
	}
	glade_xml_signal_connect_data (swidget->xml, "on_details_enable_toggle",
									G_CALLBACK (trust_enable_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_details_trust_changed",
								G_CALLBACK (trust_changed), swidget);
}

static void 
do_details (SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;

	gpgme_subkey_t subkey;

	GtkWidget *widget;
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeIter default_key;
	GtkTreeSelection *selection;
	gchar dbuffer[G_ASCII_DTOSTR_BUF_SIZE];
	gchar *fp_label;
	const gchar *label, *status, *expiration_date, *length;
	gint subkey_number, key, trust;

	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);
	subkey = pkey->pubkey->subkeys;
	
	widget = glade_xml_get_widget (swidget->xml, "details-id-label");
	label = seahorse_key_get_keyid (skey); 
	if (strlen (label) > 8)
		label += 8;
	gtk_label_set_text (GTK_LABEL (widget), label);
	
	widget = glade_xml_get_widget (swidget->xml, "details-fingerprint-label");
	fp_label = seahorse_key_get_fingerprint (skey);  
	fp_label[24] = '\n';
	gtk_label_set_text (GTK_LABEL (widget), fp_label);

	widget = glade_xml_get_widget (swidget->xml, "details-algo-label");
	label = seahorse_pgp_key_get_algo (pkey, 0);
	gtk_label_set_text (GTK_LABEL (widget), label);
	
	widget = glade_xml_get_widget (swidget->xml, "details-created-label");
	label = seahorse_util_get_date_string (subkey->timestamp); 
	gtk_label_set_text (GTK_LABEL (widget), label);

	widget = glade_xml_get_widget (swidget->xml, "details-strength-label");
	g_ascii_dtostr(dbuffer, G_ASCII_DTOSTR_BUF_SIZE, subkey->length);
	gtk_label_set_text (GTK_LABEL (widget), dbuffer);
	
	widget = glade_xml_get_widget (swidget->xml, "details-expires-label");
	label = seahorse_util_get_date_string (subkey->expires);
	if (g_str_equal("0", label))
		label = _("Never");
	gtk_label_set_text (GTK_LABEL (widget), label);
	
	widget = glade_xml_get_widget (swidget->xml, "details-enable-toggle");	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !(pkey->pubkey->disabled));
	
	widget = glade_xml_get_widget (swidget->xml, "details-trust-combobox");
	gtk_widget_set_sensitive (widget, !(pkey->pubkey->disabled));
		
	trust = seahorse_key_get_trust (skey);

	if ( 0 < trust ) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget),  trust-1);
	}
	
	/* Clear/create table store */
	widget = glade_xml_get_widget (swidget->xml, "details-subkey-tree");
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	
	if (!store) {
		/* This is our first time so create a store */
		store = gtk_list_store_new (SUBKEY_N_COLUMNS,
									G_TYPE_INT, /*index */
									G_TYPE_STRING,   /* id */
									G_TYPE_STRING, 	/* created */
									G_TYPE_STRING, 	/* expires */
									G_TYPE_STRING, 	/* status  */
									G_TYPE_STRING,	/* type */
									G_TYPE_STRING 	/* length*/
									); 
																		 
		/* Make the columns for the view */
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
					 -1,
					 "ID",
					 gtk_cell_renderer_text_new (), 
					 "text", SUBKEY_ID, 
					 NULL);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
					-1,
					"Type",
					gtk_cell_renderer_text_new (), 
					"text", SUBKEY_TYPE, 
					NULL);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
					-1,
					"Created",
					gtk_cell_renderer_text_new (), 
					"text", SUBKEY_CREATED, 
					NULL);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
					-1,
					"Expires",
					gtk_cell_renderer_text_new (), 
					"text", SUBKEY_EXPIRES, 
					NULL);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
					-1,
					"Status",
					gtk_cell_renderer_text_new (), 
					"text", SUBKEY_STATUS, 
					NULL);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
					-1,
					"Strength",
					gtk_cell_renderer_text_new (), 
					"text", SUBKEY_LENGTH, 
					NULL);
	} else 	
		gtk_list_store_clear (store);
	
	subkey_number = seahorse_pgp_key_get_num_subkeys (pkey);
	for (key = 0; key < subkey_number; key++) {
		
		subkey = seahorse_pgp_key_get_nth_subkey (pkey, key);
		
		status = subkey->revoked ? "Revoked" : "Good";
		status = subkey->expired ? "Expired" : "Good";
		
		expiration_date = seahorse_util_get_date_string (subkey->expires);
		if (g_str_equal (expiration_date, "0"))
			expiration_date = "Never";
	
		length = g_ascii_dtostr(dbuffer, G_ASCII_DTOSTR_BUF_SIZE, subkey->length);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,  
							SUBKEY_INDEX, key,
							SUBKEY_ID, seahorse_pgp_key_get_id (pkey->pubkey, key),
							SUBKEY_TYPE, seahorse_pgp_key_get_algo (pkey, key),
							SUBKEY_CREATED, seahorse_util_get_date_string (subkey->timestamp),
							SUBKEY_EXPIRES, expiration_date,
							SUBKEY_STATUS,  status, 
							SUBKEY_LENGTH, length,
							-1);

		if (!key) {
			default_key = iter;
		}
		
	} 
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(store));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_select_iter (selection, &default_key);
	
}

static void
signatures_delete_button_clicked (GtkWidget *widget, SeahorseWidget *skey) 
{
	
}

static void
signatures_revoke_button_clicked (GtkWidget *widget, SeahorseWidget *skey)
{	}

static void
signatures_populate_model (SeahorseWidget *swidget, GtkListStore *store)
{
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	
	GtkTreeIter iter;
	GtkWidget *widget;
	GtkComboBox *combo;
	GtkToggleButton *toggle;
	gboolean keyring_only;
	const gchar *name;
	
	gpgme_user_id_t uid;
	gpgme_key_sig_t sig;
	
	keyring_only = TRUE;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);
	
	widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
	toggle = GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, "signatures-toggle"));
	keyring_only = gtk_toggle_button_get_active (toggle);
	
	combo = GTK_COMBO_BOX (glade_xml_get_widget (swidget->xml, "signatures-filter-combobox"));
	name = gtk_combo_box_get_active_text (combo);
	
	if (store) {
		/* By rule of thumb, if the sig->name is null, the key isn't in the keyring */
		for (uid = pkey->pubkey->uids; uid; uid = uid->next) {
			for (sig = uid->signatures; sig; sig = sig->next)  {
				if (keyring_only && g_str_equal (sig->name, "")) {
					//don't push the sig, do nothing
				} else {
					if (g_str_equal (name, "All User IDs")) {		
						gtk_list_store_append (store, &iter);					
						gtk_list_store_set (store, &iter, 
											SIGN_KEY_ID, sig->keyid,
											SIGN_NAME, sig->name, 
											SIGN_EMAIL, sig->email, 
											SIGN_COMMENT, sig->comment, 
											-1);
					} else if (g_str_equal (name, sig->name)) {
						gtk_list_store_append (store, &iter);
						gtk_list_store_set (store, &iter, 
											SIGN_KEY_ID, sig->keyid,
											SIGN_NAME, sig->name, 
											SIGN_EMAIL, sig->email, 
											SIGN_COMMENT, sig->comment, 
											-1);
					}				
				}	
			}
		}
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(store));

}

static void
signatures_filter_changed (GtkComboBox *combo, SeahorseWidget *swidget)
{
	
	GtkWidget *widget;
	GtkListStore *store;
	
	widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	
	if (store) {
		gtk_list_store_clear (store);
	}

	signatures_populate_model (swidget, store);

}
	
/* The signatures toggle filters the view for the user so that only signatures 
 * in the collected keys appear. This way the user won't have to filter 
 * through meaningless KeyIDs. Self signatures on personal keys should be 
 * assumed, don't display those by default either.
 */	
static void
signatures_toggled (GtkToggleButton *toggle, SeahorseWidget *swidget)
{
	
	GtkWidget *widget;
	GtkListStore *store;
	
	widget = glade_xml_get_widget (swidget->xml, "signatures-filter-combobox");
	if (!gtk_toggle_button_get_active (toggle)) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		gtk_widget_set_sensitive (widget, FALSE);
	} else {
		gtk_widget_set_sensitive (widget, TRUE);
	}
	
	widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	
	gtk_list_store_clear (store);
	signatures_populate_model (swidget, store);	
}

static void
do_signatures_signals (SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	SeahorseKeyEType etype;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	etype = seahorse_key_get_etype (skey);
	
	if (etype == SKEY_PUBLIC ) {
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "signatures-revoke-button"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "signatures-delete-button"));
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, "signatures-empty-label"));
	} else {
		glade_xml_signal_connect_data (swidget->xml, "signatures_revoke_button_clicked",
									   G_CALLBACK (signatures_revoke_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "signatures_delete_button_clicked",
									   G_CALLBACK (signatures_delete_button_clicked), swidget);
	}

	glade_xml_signal_connect_data (swidget->xml, "on_signatures_toggle_toggled",
								   G_CALLBACK (signatures_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_signatures_combobox_changed",
								   G_CALLBACK (signatures_filter_changed), swidget);
	
}	

static void 
do_signatures (SeahorseWidget *swidget)
{
	
	SeahorseKey *skey;
	SeahorsePGPKey *pkey;
	
	GtkWidget *widget;
	GtkListStore *store;
	GArray *names;
	gint names_size, i, j;
	const gchar *name;
	
	gpgme_user_id_t uid;
	gpgme_key_sig_t sig;

	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	pkey = SEAHORSE_PGP_KEY (skey);

	widget = glade_xml_get_widget (swidget->xml, "signatures-filter-combobox");
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	
	names = g_array_new (FALSE, FALSE, sizeof (gchar*));
	names_size = 0;
	for (uid = pkey->pubkey->uids; uid; uid = uid->next) {
		for (sig = uid->signatures; sig; sig = sig->next)  {	
			if (!g_str_equal (sig->name, "")) {
				g_array_append_val (names, sig->name);
				names_size++;
			}
		}
	}
	
	for (i = 0; i < names_size; i++) {
		name = g_array_index (names, gchar*, i);
		for (j = i+1; j < names_size; j++) {
			if (g_str_equal (name, g_array_index (names, gchar*, j))) {
				g_array_remove_index (names, j);
				names_size--;
				j--;
			} 
		}
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), name);
	}
	
	widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
	store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
	
	if (!store) {
		store = gtk_list_store_new (SIGN_N_COLUMNS,
									G_TYPE_STRING,
									G_TYPE_STRING,
									G_TYPE_STRING,
									G_TYPE_STRING
									);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
					 -1,
					 "ID",
					 gtk_cell_renderer_text_new (), 
					 "text", SIGN_KEY_ID, 
					 NULL);
		 gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
					 -1,
					 "Name",
					 gtk_cell_renderer_text_new (), 
					 "text", SIGN_NAME, 
					 NULL);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
					 -1,
					 "Email",
					 gtk_cell_renderer_text_new (), 
					 "text", SIGN_EMAIL, 
					 NULL);
		gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
					 -1,
					 "Comment",
					 gtk_cell_renderer_text_new (), 
					 "text", SIGN_COMMENT, 
					 NULL);
	} else {
		gtk_list_store_clear (store);
	}
	signatures_populate_model (swidget, store);
	g_array_free (names, TRUE);
}

/**
 * update the property dialogs if properties of the key changed
 **/
static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseWidget *swidget)
{
	switch (change) {
		case SKEY_CHANGE_ALL:
			break;
		case SKEY_CHANGE_PHOTOS:
		case SKEY_CHANGE_UIDS:
			do_owner (swidget);
			/* in some cases, when the uids change, the signature 
			   page needs to be refreshed. For example, if a userid
		       is changed to primary, the key is re-selfsigned w/ the
			   new userid */
		
			//do_signatures (swidget);
			break;
		case SKEY_CHANGE_SIGNERS:
			do_signatures (swidget);
			break;
		case SKEY_CHANGE_SUBKEYS: 
			do_details (swidget);
			break;
		case SKEY_CHANGE_EXPIRES:
			do_details (swidget);
			break;
		default:
			break;
	}
}

static void
key_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
	seahorse_widget_destroy (swidget);
}

static void
properties_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
	g_signal_handlers_disconnect_by_func (SEAHORSE_KEY_WIDGET (swidget)->skey,
										  key_changed, swidget);
	g_signal_handlers_disconnect_by_func (SEAHORSE_KEY_WIDGET (swidget)->skey,
										  key_destroyed, swidget);
}

void
seahorse_key_properties_new (SeahorsePGPKey *pkey)
{
	SeahorseKey *skey = SEAHORSE_KEY (pkey);
	SeahorseKeySource *sksrc;
	SeahorseWidget *swidget;
	GtkWidget *widget;
	gboolean remote;
	
	remote = seahorse_key_get_location (skey) == SKEY_LOC_REMOTE;
	
	/* Reload the key to make sure to get all the props */
	sksrc = seahorse_key_get_source (skey);
	g_return_if_fail (sksrc != NULL);
		
	/* Don't trigger the import of remote keys if possible */
	if (!remote) {
		/* This causes the key source to get any specific info about the key */
		seahorse_key_source_load_sync (sksrc, SKSRC_LOAD_KEY, seahorse_key_get_keyid (skey));
		skey = seahorse_context_get_key (SCTX_APP(), sksrc, seahorse_key_get_keyid (skey));
		g_return_if_fail (skey != NULL);                
	}
	
	swidget = seahorse_key_widget_new ("key-properties", skey);
	
	/* This happens if the window is already open */
	if (swidget == NULL)
		return;
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
	g_signal_connect_after (skey, "changed", G_CALLBACK (key_changed), swidget);
	g_signal_connect_after (skey, "destroy", G_CALLBACK (key_destroyed), swidget);
	
	widget = glade_xml_get_widget (swidget->xml, "signatures-toggle");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	
	
	/* The signals don't need to keep getting connected. Everytime a key changes the
	 * do_* functions get called. Therefore, seperate functions connect the signals
	 * have been created
	 */

	do_owner (swidget);
	do_owner_signals (swidget);
	
	do_details (swidget);
	do_details_signals (swidget);

	do_signatures (swidget);
	do_signatures_signals (swidget);

	
	widget = glade_xml_get_widget (swidget->xml, "notebook1");
	gtk_widget_hide (gtk_notebook_get_nth_page (GTK_NOTEBOOK (widget), 3));
	
	seahorse_widget_show (swidget);    
}
