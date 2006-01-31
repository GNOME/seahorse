/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Nate Nielsen
 * Copyright (C) 2005 Jim Pharis
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
#include "seahorse-key.h"
#include "seahorse-pgp-key.h"
#include "seahorse-pgp-key-op.h"
#include "seahorse-gtkstock.h"
#include "seahorse-windows.h"

#define NOTEBOOK "notebook"
#define SPACING 12
#define ALL_UIDS _("All User IDs")

/* owner uid list */
enum {
	UID_INDEX,
	UID_NAME,
	UID_EMAIL,
	UID_COMMENT,
	UID_N_COLUMNS
};

const GType uid_columns[] = {
    G_TYPE_INT,     /* index */
    G_TYPE_STRING,  /* name */
    G_TYPE_STRING,  /* email */
    G_TYPE_STRING   /* comment */
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

const GType subkey_columns[] = {
    G_TYPE_INT,     /* index */
    G_TYPE_STRING,  /* id */
    G_TYPE_STRING,  /* created */
    G_TYPE_STRING,  /* expires */
    G_TYPE_STRING,  /* status  */
    G_TYPE_STRING,  /* type */
    G_TYPE_STRING   /* length*/
};

/* signatures */
enum {
	SIGN_KEY_ID,
	SIGN_NAME,
	SIGN_EMAIL,
	SIGN_COMMENT,
	SIGN_N_COLUMNS
};

const GType sign_columns[] = {
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING,
    G_TYPE_STRING
};

static void 
show_glade_widget (SeahorseWidget *swidget, const gchar *name, gboolean show)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, name);
    if (!widget)
        return;
    
    if (show)
        gtk_widget_show (widget);
    else
        gtk_widget_hide (widget);
}

static void
set_glade_image (SeahorseWidget *swidget, const gchar *name, const gchar *file)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, name);
    gchar *t;
    
    if (!widget)
        return;
    
    t = g_strdup_printf (DATA_DIR "/pixmaps/seahorse/%s", file);
    gtk_image_set_from_file (GTK_IMAGE (widget), t);
    g_free (t);
}

static void
sensitive_glade_widget (SeahorseWidget *swidget, const gchar *name, gboolean sens)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, name);
    if (widget)
        gtk_widget_set_sensitive (widget, sens);
}    

static gint
get_selected_row (SeahorseWidget *swidget, const gchar *gladeid, guint column)
{
    GtkTreeSelection *selection;
    GtkWidget *widget;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GList *rows;
    gint index = -1;

    widget = glade_xml_get_widget (swidget->xml, gladeid);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    g_assert (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE);

    rows = gtk_tree_selection_get_selected_rows (selection, NULL);

    if (g_list_length (rows) > 0) {
        gtk_tree_model_get_iter (model, &iter, rows->data);
        gtk_tree_model_get (model, &iter, column, &index, -1);
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

    return index;
}

static gint
get_selected_uid (SeahorseWidget *swidget)
{
    return get_selected_row (swidget, "owner-userid-tree", UID_INDEX);
}

static gint 
get_selected_subkey (SeahorseWidget *swidget)
{
    return get_selected_row (swidget, "details-subkey-tree", SUBKEY_INDEX);
}

static void
help_button_clicked(GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_widget_show_help(swidget);
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
	} 
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
    SeahorseKey *skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    gint index = get_selected_uid (swidget);
    
    if (index >= 1) 
        seahorse_delete_userid_show (skey, index);
}

static void
owner_photo_add_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gchar* filename = NULL;
    gpgme_error_t gerr;
    GtkWidget *chooser;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    
    chooser = seahorse_util_chooser_open_new (_("Add Photo ID"), 
                                              GTK_WINDOW (seahorse_widget_get_top (swidget)));
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);		
    seahorse_util_chooser_show_jpeg_files (chooser);

    if(gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
        
    gtk_widget_destroy (chooser);

    g_return_if_fail (filename);

    gerr = seahorse_pgp_key_op_photoid_add (pkey, filename);
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
    g_return_if_fail (photoid != NULL);
    
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
    
    if (response != GTK_RESPONSE_ACCEPT)
        return;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    
    uid = photoid->uid;
    
    gerr = seahorse_pgp_key_op_photoid_delete (pkey, uid);
    if (!GPG_IS_OK (gerr))
        seahorse_util_handle_gpgme (gerr, _("Couldn't delete photo"));
    
    g_object_set_data (G_OBJECT (swidget), "current-photoid", NULL);
}

static void
owner_photo_primary_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgme_error_t gerr;
    gint uid;
    gpgmex_photo_id_t photoid;

    photoid	= (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");
    g_return_if_fail (photoid != NULL);
        
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    
    uid = photoid->uid;
    
    gerr = seahorse_pgp_key_op_photoid_primary (pkey, uid);
    if (!GPG_IS_OK (gerr))
        seahorse_util_handle_gpgme (gerr, _("Couldn't change primary photo ID"));
}

static void
set_photoid_state(SeahorseWidget *swidget, SeahorsePGPKey *pkey)
{
    SeahorseKeyEType etype; 
    GtkWidget *photo_image;
    guint num_photoids;

    gpgmex_photo_id_t photoid;

    etype = seahorse_key_get_etype (SEAHORSE_KEY(pkey));
    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");

    /* Sensitive when adding a photo is possible */
    sensitive_glade_widget (swidget, "owner-photo-add-button", 
                            etype == SKEY_PRIVATE);

    /* Sensitive when we have a photo to set as primary */
    sensitive_glade_widget (swidget, "owner-photo-primary-button", 
                            etype == SKEY_PRIVATE && photoid);

    /* Sensitive when we have a photo id to delete */
    sensitive_glade_widget (swidget, "owner-photo-delete-button", 
                            etype == SKEY_PRIVATE && photoid);
    
    /* Sensitive when not the first photo id */
    sensitive_glade_widget (swidget, "owner-photo-previous-button", 
                            photoid && photoid != pkey->photoids);
    
    sensitive_glade_widget (swidget, "owner-photo-first-button", 
                            photoid && photoid != pkey->photoids);
    
    /* Sensitive when not the last photo id */
    sensitive_glade_widget (swidget, "owner-photo-next-button", 
                            photoid && photoid->next && photoid->next->photo);
                            
    sensitive_glade_widget (swidget, "owner-photo-last-button",
                            photoid && photoid->next && photoid->next->photo);
                            
    /* Display this when there are any photo ids */
    show_glade_widget (swidget, "owner-photo-delete-button", photoid != NULL);
    
    /* Display these when there are more than one photo id */
    show_glade_widget (swidget, "owner-photo-previous-button", 
                       pkey->photoids && pkey->photoids->next);
    show_glade_widget (swidget, "owner-photo-next-button", 
                       pkey->photoids && pkey->photoids->next);
                       
    /* Display these when there are more than 2 photo ids */
    num_photoids = seahorse_pgp_key_get_num_photoids(pkey);
    
    show_glade_widget (swidget, "owner-photo-first-button",
                       (num_photoids > 2));
    show_glade_widget (swidget, "owner-photo-last-button",
                       (num_photoids > 2));
        
    photo_image = glade_xml_get_widget (swidget->xml, "photoid");
    if (photo_image) {
        if (!photoid || !photoid->photo)
            gtk_image_set_from_stock (GTK_IMAGE (photo_image), SEAHORSE_STOCK_PERSON, (GtkIconSize)-1);
        else 
            gtk_image_set_from_pixbuf (GTK_IMAGE (photo_image), photoid->photo);
    }
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
photoid_next_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t photoid;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");

    if (photoid && photoid->next)
        g_object_set_data (G_OBJECT (swidget), "current-photoid", photoid->next);
        
    set_photoid_state (swidget, pkey);
}

static void
photoid_prev_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t itter, photoid;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    photoid = (gpgmex_photo_id_t)g_object_get_data (G_OBJECT (swidget), "current-photoid");

    if (photoid != pkey->photoids) {
        itter = pkey->photoids;
    
        while (itter->next != photoid && itter->next)
           itter = itter->next;
    
        g_object_set_data (G_OBJECT (swidget), "current-photoid", itter);
    }

    set_photoid_state (swidget, pkey);
}

static void
photoid_first_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t photoid;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    photoid = pkey->photoids;
    
    g_object_set_data (G_OBJECT (swidget), "current-photoid", photoid);
    set_photoid_state (swidget, pkey);
}

static void
photoid_last_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgmex_photo_id_t photoid;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    photoid = pkey->photoids;
    
    while(photoid->next != NULL)
        photoid = photoid->next;
    
    g_object_set_data (G_OBJECT (swidget), "current-photoid", photoid);
    set_photoid_state (swidget, pkey);
}

static void
photoid_button_pressed(GtkWidget *widget, GdkEvent *event, SeahorseWidget *swidget)
{
    GdkEventScroll *event_scroll;
    
    if(event->type == GDK_SCROLL){
        event_scroll = (GdkEventScroll *) event;
        
        if(event_scroll->direction == GDK_SCROLL_UP)
            photoid_prev_clicked (widget, swidget);
        else if(event_scroll->direction == GDK_SCROLL_DOWN)
            photoid_next_clicked(widget, swidget);
    }
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
	glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_next_clicked",
								G_CALLBACK (photoid_next_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_previous_clicked",
								G_CALLBACK (photoid_prev_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_first_button_clicked",
	                            G_CALLBACK (photoid_first_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "on_owner_photo_last_button_clicked",
	                            G_CALLBACK (photoid_last_clicked), swidget);
    glade_xml_signal_connect_data(swidget->xml, "on_image_eventbox_scroll_event", 
                                G_CALLBACK(photoid_button_pressed), swidget);
	
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
        show_glade_widget (swidget, "owner-add-button", FALSE);
        show_glade_widget (swidget, "owner-delete-button", FALSE);
        show_glade_widget (swidget, "owner-primary-button", FALSE);
        show_glade_widget (swidget, "owner-photo-add-button", FALSE);
        show_glade_widget (swidget, "owner-photo-delete-button", FALSE);
        show_glade_widget (swidget, "owner-photo-primary-button", FALSE);
	}
}

static void	
do_owner (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    gpgme_user_id_t uid;
    GtkWidget *widget;
    GtkListStore *store;
    GtkTreeIter iter;
    gchar *text, *t;
    gulong expires_date;
    guint flags;
    const gchar *name, *email, *comment;
    int i;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    flags = seahorse_key_get_flags (skey);
    
    /* Display appropriate warnings */    
    show_glade_widget (swidget, "expired-area", flags & SKEY_FLAG_EXPIRED);
    show_glade_widget (swidget, "revoked-area", flags & SKEY_FLAG_REVOKED);
    show_glade_widget (swidget, "disabled-area", flags & SKEY_FLAG_DISABLED);
    
    /* Update the expired message */
    if (flags & SKEY_FLAG_EXPIRED) {
        widget = glade_xml_get_widget (swidget->xml, "expired-message");
        if (widget) {
            
            expires_date = seahorse_key_get_expires (skey);
            if (expires_date == 0) 
                t = g_strdup (_("(unknown)"));
            else
                t = seahorse_util_get_date_string (expires_date);
            text = g_strdup_printf (_("This key expired on: %s"), t);
                
            gtk_label_set_text (GTK_LABEL (widget), text);
            
            g_free (t);
            g_free (text);
        }
    }
        
    /* Hide trust page when above */
    show_glade_widget (swidget, "trust-page", !((flags & SKEY_FLAG_EXPIRED) ||
                       (flags & SKEY_FLAG_REVOKED) || (flags & SKEY_FLAG_DISABLED)));

    /* Hide or show the uids area */
    show_glade_widget (swidget, "uids-area", seahorse_pgp_key_get_num_userids (pkey) > 1);

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
    
    widget = glade_xml_get_widget (swidget->xml, "owner-keyid-label");
    if (widget)
        gtk_label_set_text (GTK_LABEL (widget), seahorse_key_get_short_keyid (skey)); 
    
    /* Clear/create table store */
    widget = glade_xml_get_widget (swidget->xml, "owner-userid-tree");
    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));	

    if (store) {
        gtk_list_store_clear (GTK_LIST_STORE (store));
        
    } else {

        /* This is our first time so create a store */
        store = gtk_list_store_newv (UID_N_COLUMNS, (GType*)uid_columns);

        /* Make the columns for the view */
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("Name"), gtk_cell_renderer_text_new (), 
                                                     "text", UID_NAME, NULL);

        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Email"), gtk_cell_renderer_text_new (), 
                                                     "text", UID_EMAIL, NULL);

        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Comment"), gtk_cell_renderer_text_new (), 
                                                     "text", UID_COMMENT, NULL);
    }

    for (i = 1, uid = pkey->pubkey->uids; uid; uid = uid->next, i++) {

        name = seahorse_pgp_key_get_userid_name (pkey, i - 1);
        email = seahorse_pgp_key_get_userid_email (pkey, i - 1);
        comment = seahorse_pgp_key_get_userid_comment (pkey, i - 1);

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
			seahorse_util_handle_gpgme (err, _("Unable to change trust"));
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
	                                         GTK_WINDOW (seahorse_widget_get_top (swidget)));
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
details_calendar_button_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint index;
	
	g_return_if_fail (SEAHORSE_IS_KEY_WIDGET (swidget));
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;

	index = get_selected_subkey (swidget);
	
	if (-1 == index) {
		index = 0;
	}
	seahorse_expires_new (SEAHORSE_PGP_KEY (skey), index);	
}

static void
do_details_signals (SeahorseWidget *swidget) 
{ 
	SeahorseKey *skey;
	SeahorseKeyEType etype;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	etype = seahorse_key_get_etype (skey);
	
	/* 
	 * if not the key owner, disable most everything
	 * if key owner, add the callbacks to the subkey buttons
	 */
	 if (etype == SKEY_PUBLIC ) {
         show_glade_widget (swidget, "details-actions-label", FALSE);
         show_glade_widget (swidget, "details-passphrase-button", FALSE);
         show_glade_widget (swidget, "details-export-button", FALSE);
         show_glade_widget (swidget, "details-add-button", FALSE);
         show_glade_widget (swidget, "details-date-button", FALSE);
         show_glade_widget (swidget, "details-revoke-button", FALSE);
         show_glade_widget (swidget, "details-delete-button", FALSE);
		 show_glade_widget (swidget, "details-calendar-button", FALSE);
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
		glade_xml_signal_connect_data (swidget->xml, "on_details_calendar_button_clicked",
									   G_CALLBACK (details_calendar_button_clicked), swidget);
		glade_xml_signal_connect_data (swidget->xml, "on_details_export_button_clicked",
										G_CALLBACK (details_export_button_clicked), swidget);
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
    gchar *fp_label, *expiration_date, *created_date;
    const gchar *label, *status, *length;
    gint subkey_number, key, trust;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    subkey = pkey->pubkey->subkeys;
			
    widget = glade_xml_get_widget (swidget->xml, "details-id-label");
    if (widget) {
        label = seahorse_key_get_short_keyid (skey); 
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-fingerprint-label");
    if (widget) {
        fp_label = seahorse_key_get_fingerprint (skey);  
        if (strlen (fp_label) > 24)
            fp_label[24] = '\n';
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-algo-label");
    if (widget) {
        label = seahorse_pgp_key_get_algo (pkey, 0);
        gtk_label_set_text (GTK_LABEL (widget), label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-created-label");
    if (widget) {
        fp_label = seahorse_util_get_date_string (subkey->timestamp); 
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-strength-label");
    if (widget) {
        g_ascii_dtostr (dbuffer, G_ASCII_DTOSTR_BUF_SIZE, subkey->length);
        gtk_label_set_text (GTK_LABEL (widget), dbuffer);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-expires-label");
    if (widget) {
        if (subkey->expires == 0)
            fp_label = g_strdup (_("Never"));
        else
            fp_label = seahorse_util_get_date_string (subkey->expires);
        gtk_label_set_text (GTK_LABEL (widget), fp_label);
        g_free (fp_label);
    }

    widget = glade_xml_get_widget (swidget->xml, "details-enable-toggle");
    if (widget)
         gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !(pkey->pubkey->disabled));

    widget = glade_xml_get_widget (swidget->xml, "details-trust-combobox");
    if (widget) {
        gtk_widget_set_sensitive (widget, !(pkey->pubkey->disabled));
        trust = seahorse_key_get_trust (skey);
        if (0 < trust)
            gtk_combo_box_set_active (GTK_COMBO_BOX (widget),  trust - 1);
    }

    /* Clear/create table store */
    widget = glade_xml_get_widget (swidget->xml, "details-subkey-tree");
    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));

    if (store) {
        gtk_list_store_clear (store);
        
    } else {
        
        /* This is our first time so create a store */
        store = gtk_list_store_newv (SUBKEY_N_COLUMNS, (GType*)subkey_columns);

        /* Make the columns for the view */
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("ID"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_ID, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Type"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_TYPE, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Created"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_CREATED, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Expires"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_EXPIRES, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Status"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_STATUS, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget),
                                                     -1, _("Strength"), gtk_cell_renderer_text_new (), 
                                                     "text", SUBKEY_LENGTH, NULL);
    }

    subkey_number = seahorse_pgp_key_get_num_subkeys (pkey);
    for (key = 0; key < subkey_number; key++) {

        subkey = seahorse_pgp_key_get_nth_subkey (pkey, key);

        status = subkey->revoked ? _("Revoked") : _("Good");
        status = subkey->expired ? _("Expired") : _("Good");

        if (subkey->expires == 0)
            expiration_date = g_strdup (_("Never"));
        else
            expiration_date = seahorse_util_get_date_string (subkey->expires);

        created_date = seahorse_util_get_date_string (subkey->timestamp);
        
        length = g_ascii_dtostr(dbuffer, G_ASCII_DTOSTR_BUF_SIZE, subkey->length);

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,  
                            SUBKEY_INDEX, key,
                            SUBKEY_ID, seahorse_pgp_key_get_id (pkey->pubkey, key),
                            SUBKEY_TYPE, seahorse_pgp_key_get_algo (pkey, key),
                            SUBKEY_CREATED, created_date,
                            SUBKEY_EXPIRES, expiration_date,
                            SUBKEY_STATUS,  status, 
                            SUBKEY_LENGTH, length,
                            -1);
        
        g_free (expiration_date);
        g_free (created_date);

        if (!key) 
            default_key = iter;
    } 
    
    gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL(store));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    gtk_tree_selection_select_iter (selection, &default_key);
}

static void
signatures_delete_button_clicked (GtkWidget *widget, SeahorseWidget *skey) 
{
    /* TODO: */
}

static void
signatures_revoke_button_clicked (GtkWidget *widget, SeahorseWidget *skey)
{
    /* TODO: */
}

static void
signatures_populate_model (SeahorseWidget *swidget, GtkListStore *store)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    GtkTreeIter iter;
    GtkWidget *widget;
    gboolean trusted_only = TRUE;
    const gchar *name = NULL;
    const gchar *keyid;
    gpgme_user_id_t uid;
    gpgme_key_sig_t sig;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);

    widget = glade_xml_get_widget (swidget->xml, "signatures-toggle");
    if (widget)
        trusted_only = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    
    widget =  glade_xml_get_widget (swidget->xml, "signatures-filter-combobox");
    if (widget) {
        name = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
        if (g_str_equal (name, ALL_UIDS)) 
            name = NULL;
    }

    widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
    
    if (store) {
        keyid = seahorse_pgp_key_get_id (pkey->pubkey, 0);

        for (uid = pkey->pubkey->uids; uid; uid = uid->next) {
            for (sig = uid->signatures; sig; sig = sig->next) {
                
                /* Never show self signatures, they're implied */
                if (strcmp (sig->keyid, keyid) == 0)
                    continue;
                
                /* Find any trusted signatures */
                if (trusted_only && !(seahorse_pgp_key_get_sigtype (pkey, sig) & SKEY_PGPSIG_TRUSTED))
                    continue;

                gtk_list_store_append (store, &iter);
                
                if (!name)
                    gtk_list_store_set (store, &iter, 
                                        SIGN_KEY_ID, sig->keyid,
                                        SIGN_NAME, sig->name, 
                                        SIGN_EMAIL, sig->email, 
                                        SIGN_COMMENT, sig->comment, 
                                        -1);
                
                else if (g_str_equal (name, sig->name))
                    gtk_list_store_set (store, &iter, 
                                        SIGN_KEY_ID, sig->keyid,
                                        SIGN_NAME, sig->name, 
                                        SIGN_EMAIL, sig->email, 
                                        SIGN_COMMENT, sig->comment, 
                                        -1);
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
	
	if (store)
		gtk_list_store_clear (store);

	signatures_populate_model (swidget, store);
}

/* 
 * The signatures toggle filters the view for the user so that only signatures 
 * in the collected keys appear. This way the user won't have to filter 
 * through meaningless KeyIDs. Self signatures on personal keys should be 
 * assumed, don't display those by default either.
 */
static void
trust_toggled (GtkToggleButton *toggle, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    GtkListStore *store;

    widget = glade_xml_get_widget (swidget->xml, "signatures-filter-combobox");
    if (widget) {
        if (!gtk_toggle_button_get_active (toggle)) {
            gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
            gtk_widget_set_sensitive (widget, FALSE);
        } else {
            gtk_widget_set_sensitive (widget, TRUE);
        }
    }

    widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));

    gtk_list_store_clear (store);
    signatures_populate_model (swidget, store);	
}

static void 
trust_sign_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    GList *keys;

    keys = g_list_append (NULL, SEAHORSE_KEY_WIDGET (swidget)->skey);
    seahorse_sign_show (keys);
    g_list_free (keys);
}

static void
do_trust_signals (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorseKeyEType etype;
    GtkWidget *widget;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    etype = seahorse_key_get_etype (skey);
    
    set_glade_image (swidget, "image-good1", "seahorse-good.png");
    set_glade_image (swidget, "image-good2", "seahorse-good.png");
    
    glade_xml_signal_connect_data (swidget->xml, "trust_sign_clicked", 
                                   G_CALLBACK (trust_sign_clicked), swidget);
    
    if (etype == SKEY_PUBLIC ) {
        show_glade_widget (swidget, "signatures-revoke-button", FALSE);
        show_glade_widget (swidget, "signatures-delete-button", FALSE);
        show_glade_widget (swidget, "signatures-empty-label", FALSE);
    } else {
        glade_xml_signal_connect_data (swidget->xml, "signatures_revoke_button_clicked",
                                       G_CALLBACK (signatures_revoke_button_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "signatures_delete_button_clicked",
                                       G_CALLBACK (signatures_delete_button_clicked), swidget);
    }

    glade_xml_signal_connect_data (swidget->xml, "on_signatures_toggle_toggled",
                                   G_CALLBACK (trust_toggled), swidget);
    glade_xml_signal_connect_data (swidget->xml, "on_signatures_combobox_changed",
                                   G_CALLBACK (signatures_filter_changed), swidget);

    widget = glade_xml_get_widget (swidget->xml, "signatures-toggle");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);	
}

static void 
do_trust (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    SeahorsePGPKey *pkey;
    GtkWidget *widget;
    GtkListStore *store;
    GArray *names;
    gint names_size, i, j;
    const gchar *name;
    guint trust;
    gboolean trusted;
    gboolean sigpersonal;
    gpgme_user_id_t uid;
    gpgme_key_sig_t sig;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    pkey = SEAHORSE_PGP_KEY (skey);
    
    trusted = seahorse_key_get_flags (skey) & SKEY_FLAG_TRUSTED;
    sigpersonal = seahorse_pgp_key_have_signatures (pkey, SKEY_PGPSIG_PERSONAL);
    trust = seahorse_key_get_trust (skey);
    
    show_glade_widget (swidget, "untrusted-area", !sigpersonal);
    show_glade_widget (swidget, "untrusted-overridden-area", ((trust != SEAHORSE_VALIDITY_NEVER)||(trust != SEAHORSE_VALIDITY_UNKNOWN)));
    show_glade_widget (swidget, "trusted-signed-area", trusted && sigpersonal);
    show_glade_widget (swidget, "trusted-area", trusted && !sigpersonal);

    widget = glade_xml_get_widget (swidget->xml, "signatures-filter-combobox");

    /* Fill in the signatures filter combo box */
    if (widget) {
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
        
        g_array_free (names, TRUE);
    }

    /* The actual signatures listing */
    widget = glade_xml_get_widget (swidget->xml, "signatures-tree");
    store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    
    if (store) {
        gtk_list_store_clear (store);
    
    /* First time create the store */
    } else {
        store = gtk_list_store_newv (SIGN_N_COLUMNS, (GType*)sign_columns);

        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("ID"), gtk_cell_renderer_text_new (), 
                                                     "text", SIGN_KEY_ID, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("Name"), gtk_cell_renderer_text_new (), 
                                                     "text", SIGN_NAME, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("Email"), gtk_cell_renderer_text_new (), 
                                                     "text", SIGN_EMAIL, NULL);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (widget), 
                                                     -1, _("Comment"), gtk_cell_renderer_text_new (), 
                                                     "text", SIGN_COMMENT, NULL);
    } 

    signatures_populate_model (swidget, store);
}

/**
 * update the property dialogs if properties of the key changed
 **/
static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseWidget *swidget)
{
    switch (change) {
    case SKEY_CHANGE_PHOTOS:
    case SKEY_CHANGE_UIDS:
        do_owner (swidget);
        do_trust (swidget);
        break;
    case SKEY_CHANGE_SIGNERS:
        do_trust (swidget);
        break;
    case SKEY_CHANGE_SUBKEYS: 
        do_details (swidget);
        break;
    case SKEY_CHANGE_EXPIRES:
        do_details (swidget);
        break;
    case SKEY_CHANGE_TRUST:
        do_trust (swidget);
    default:
        do_owner (swidget);
        do_trust (swidget);
        do_details (swidget);
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

static SeahorseWidget*
setup_public_properties (SeahorsePGPKey *pkey)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;

    swidget = seahorse_key_widget_new ("pgp-public-key-properties", SEAHORSE_KEY (pkey));    
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return NULL;

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
    g_signal_connect_after (pkey, "changed", G_CALLBACK (key_changed), swidget);
    g_signal_connect_after (pkey, "destroy", G_CALLBACK (key_destroyed), swidget);

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    do_owner (swidget);
    do_owner_signals (swidget);

    do_details (swidget);
    do_details_signals (swidget);

    do_trust (swidget);
    do_trust_signals (swidget);        

    glade_xml_signal_connect_data ((swidget->xml), "on_pgp_public_key_properties_help_button_clicked",
								G_CALLBACK (help_button_clicked), swidget);

    return swidget;
}

static SeahorseWidget*
setup_private_properties (SeahorsePGPKey *pkey)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;

    swidget = seahorse_key_widget_new ("key-properties", SEAHORSE_KEY (pkey));

    /* This happens if the window is already open */
    if (swidget == NULL)
        return NULL;

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
    g_signal_connect_after (pkey, "changed", G_CALLBACK (key_changed), swidget);
    g_signal_connect_after (pkey, "destroy", G_CALLBACK (key_destroyed), swidget);

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    do_owner (swidget);
    do_owner_signals (swidget);

    do_details (swidget);
    do_details_signals (swidget);

    do_trust (swidget);
    do_trust_signals (swidget);

    glade_xml_signal_connect_data ((swidget->xml), "on_key_properties_help_button_clicked",
								G_CALLBACK (help_button_clicked), swidget);

    widget = glade_xml_get_widget (swidget->xml, NOTEBOOK);
    gtk_widget_hide (gtk_notebook_get_nth_page (GTK_NOTEBOOK (widget), 3));

    return swidget;
}

void
seahorse_key_properties_new (SeahorsePGPKey *pkey)
{
    SeahorseKey *skey = SEAHORSE_KEY (pkey);
    SeahorseKeySource *sksrc;
    SeahorseWidget *swidget;
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
    
    if (seahorse_key_get_etype (skey) == SKEY_PUBLIC)
        swidget = setup_public_properties (pkey);
    else
        swidget = setup_private_properties (pkey);       
    
    if (swidget)
        seahorse_widget_show (swidget);
}
