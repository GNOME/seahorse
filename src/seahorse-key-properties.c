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
#include <time.h>

#include "seahorse-gpgmex.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-key-op.h"

#define NOTEBOOK "notebook"
#define SPACING 12

static gint
get_subkey_index (SeahorseWidget *swidget)
{
    gint index = gtk_notebook_get_current_page (GTK_NOTEBOOK (
        glade_xml_get_widget (swidget->xml, NOTEBOOK))) - 3;
    g_return_val_if_fail (index >= 0, -1);
    return index;        
}


static void
trust_changed (GtkOptionMenu *optionmenu, SeahorseWidget *swidget)
{
	gint trust;
	SeahorseKey *skey;
	
	trust = gtk_option_menu_get_history (optionmenu) + 1;
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	if (seahorse_key_get_trust (skey) != trust)
		seahorse_key_op_set_trust (skey, trust);
}

static void
disabled_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	seahorse_key_op_set_disabled (SEAHORSE_KEY_WIDGET (swidget)->skey,
		gtk_toggle_button_get_active (togglebutton));
}

static void
do_stat_label (const gchar *label, GtkTable *table, guint left, guint top,
	       gboolean selectable, GtkTooltips *tips, const gchar *tip)
{
	GtkWidget *widget, *align;
	
	widget = gtk_label_new (label);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	gtk_label_set_selectable (GTK_LABEL (widget), selectable);
	
	align = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER (align), widget);
	
	if (tip != NULL) {
		widget = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (widget), align);
		gtk_tooltips_set_tip (tips, widget, tip, NULL);
	}
	else
		widget = align;
	
	gtk_table_attach (table, widget, left, left+1, top, top+1, GTK_FILL, 0, 0, 0);
	gtk_widget_show_all (widget);
}

static GtkWidget*
do_stat_button (gchar *label, const gchar *stock_id)
{
	GtkWidget *widget, *container;
	
	container = gtk_hbox_new (FALSE, 0);
	/* Icon */
	widget = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (container), widget);
	/* Label */
	widget = gtk_label_new_with_mnemonic (label);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	gtk_container_add (GTK_CONTAINER (container), widget);
	/* Alignment */
	widget = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (container));
	/* Button, add label & icon */
	container = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show_all (container);
	
	return container;
}

static void
key_property_labels (SeahorseWidget *swidget)
{
    gchar dbuffer[G_ASCII_DTOSTR_BUF_SIZE];
    gpgme_subkey_t subkey;
    SeahorseKey *skey;
    GtkWidget *w;
    const gchar *label;
    gchar *t, *x;

    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    subkey = skey->key->subkeys;
    
    w = glade_xml_get_widget (swidget->xml, "keyid");
    label = seahorse_key_get_keyid (skey, 0); 
    gtk_label_set_text (GTK_LABEL (w), label);
    
    w = glade_xml_get_widget (swidget->xml, "fingerprint");
    label = seahorse_key_get_fingerprint (skey);  
    gtk_label_set_text (GTK_LABEL (w), label);

    w = glade_xml_get_widget (swidget->xml, "label_type");
    label = gpgme_pubkey_algo_name(subkey->pubkey_algo);

    if (label == NULL)
        label = _("Unknown");
    else if (g_str_equal ("ElG", label))
        label = _("ElGamal");
    gtk_label_set_text (GTK_LABEL (w), label);
    
    w = glade_xml_get_widget (swidget->xml, "label_created");
    label = seahorse_util_get_date_string (subkey->timestamp); 
    gtk_label_set_text (GTK_LABEL (w), label);

    w = glade_xml_get_widget (swidget->xml, "label_strength");
    g_ascii_dtostr(dbuffer, G_ASCII_DTOSTR_BUF_SIZE, subkey->length);
    gtk_label_set_text (GTK_LABEL (w), dbuffer);
    
    w = glade_xml_get_widget (swidget->xml, "label_expires");
    label = seahorse_util_get_date_string (subkey->expires);
    if (g_str_equal("0", label))
	    label = _("Never");
    gtk_label_set_text (GTK_LABEL (w), label);
    
    w = glade_xml_get_widget (swidget->xml, "label_created");
    label = seahorse_util_get_date_string (subkey->timestamp); 
    gtk_label_set_text (GTK_LABEL (w), label);
    
    if ((t = seahorse_key_get_userid_name (skey, 0)) != NULL) {
        w = glade_xml_get_widget (swidget->xml, "label_name");
        gtk_label_set_text (GTK_LABEL (w), t);  
        g_free (t);
    }
    
    if ((t = seahorse_key_get_userid_email (skey, 0)) != NULL) {
        w = glade_xml_get_widget (swidget->xml, "label_email");
        gtk_label_set_text (GTK_LABEL (w), t);  
        g_free (t);
    }
    
    if ((t = seahorse_key_get_userid_comment (skey, 0)) != NULL) {
        w = glade_xml_get_widget (swidget->xml, "label_comment");
        gtk_label_set_text (GTK_LABEL (w), t);  
        g_free (t);
    }

    t = seahorse_key_get_userid (skey, 0);
    x = g_strdup_printf (_("%s Properties"), t);
    g_free (t);
    
    w = glade_xml_get_widget (swidget->xml, swidget->name);
    gtk_window_set_title (GTK_WINDOW (w), x);
    g_free (x);
}

static void
expiry_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    if (SEAHORSE_IS_KEY_PAIR (skey))
        seahorse_expires_new(swidget->sctx, skey, get_subkey_index(swidget));
}

static void
do_stats (SeahorseWidget *swidget, GtkTable *table, guint top, guint index, gpgme_subkey_t subkey)
{
    SeahorseKey *skey;
    GtkWidget *widget;
    GtkTooltips *tips;
    const gchar *str;
    gchar * buf;
   
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    tips = gtk_tooltips_new ();

    /* key id */
    do_stat_label (_("Key ID:"), table, 0, top, FALSE, tips, _("Key identifier"));
 
    do_stat_label (seahorse_key_get_keyid (skey, index), table, 1, top, TRUE,
      tips, subkey->keyid);
    /* type */
    do_stat_label (_("Type:"), table, 2, top, FALSE, tips, _("Algorithm"));
    str = gpgme_pubkey_algo_name(subkey->pubkey_algo);
    if (str == NULL)
        str = "Unknown";
    else if (g_str_equal ("ElG", str))
        str = "ElGamal";
    do_stat_label (str, table, 3, top, FALSE, NULL, NULL);
    /* created */
    do_stat_label (_("Created:"), table, 0, top+1, FALSE, tips, _("Key creation date"));
    do_stat_label (seahorse_util_get_date_string (subkey->timestamp),
        table, 1, top+1, FALSE, NULL, NULL);
    /* length */
    do_stat_label (_("Length:"), table, 2, top+1, FALSE, NULL, NULL);
    buf = g_strdup_printf ("%d", subkey->length);
    do_stat_label (buf, table, 3, top+1, FALSE, NULL, NULL);
    g_free(buf);
    /* status */
    do_stat_label (_("Status:"), table, 0, top+3, FALSE, NULL, NULL);
    str = _("Good");
    if (subkey->revoked)
        str = _("Revoked");
    else if (subkey->expired)
        str = _("Expired");
    do_stat_label (str, table, 1, top+3, FALSE, NULL, NULL);
    /* expires */
    do_stat_label (_("Expires:"), table, 0, top+2, FALSE, NULL, NULL);
 
    if (subkey->expires)
        do_stat_label (seahorse_util_get_date_string (subkey->expires),
                       table, 1, top+2, FALSE, NULL, NULL);
    else
        do_stat_label (_("Never"), table, 1, top+2, FALSE, NULL, NULL);
    
    if (SEAHORSE_IS_KEY_PAIR (skey)) {
        widget = gtk_button_new_with_mnemonic(_("Change Expiry Date"));
        gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_HALF);
        gtk_widget_show (widget);
        gtk_table_attach (table, widget, 2, 3, top + 2, top + 3, GTK_FILL, 0, 0, 0);
       
        g_signal_connect_after (GTK_BUTTON (widget), "clicked", 
                                G_CALLBACK (expiry_clicked), swidget);      
    }
    
}    

static void
passphrase_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    if (SEAHORSE_IS_KEY_PAIR (skey))
        seahorse_key_pair_op_change_pass (SEAHORSE_KEY_PAIR (skey));
}

static void
export_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    GtkWidget *dialog;
    gchar* uri = NULL;
    GError *err = NULL;
    GList *keys = NULL;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    if (!SEAHORSE_IS_KEY_PAIR (skey))
        return;

    keys = g_list_prepend (keys, skey);
    
    dialog = seahorse_util_chooser_save_new (_("Export Complete Key"), 
                GTK_WINDOW(seahorse_widget_get_top (swidget)));
    seahorse_util_chooser_show_key_files (dialog);
    seahorse_util_chooser_set_filename (dialog, keys);
    
    uri = seahorse_util_chooser_save_prompt (dialog);
    if(uri) {
        seahorse_op_export_file (keys, TRUE, uri, &err);

        if (err != NULL)
            seahorse_util_handle_error (err, _("Couldn't export key to \"%s\""),
                    seahorse_util_uri_get_last (uri));

        g_free (uri);
    }
    
    g_list_free (keys);
}

static void
do_signatures (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    GtkComboBox *combo;
    GtkTreeModel *model;
    gpgme_user_id_t uid;
    gchar *t;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
        
    combo = GTK_COMBO_BOX (glade_xml_get_widget (swidget->xml, "sigs"));
    model = gtk_combo_box_get_model (combo);

    /* Clear the drop down */
    gtk_list_store_clear (GTK_LIST_STORE (model));

    gtk_combo_box_append_text (combo, _("All Signatures"));
    
    for (uid = skey->key->uids; uid; uid = uid->next) {
    
        if (uid->signatures == NULL)
            continue;
            
        if (!g_utf8_validate (uid->uid, -1, NULL))
            t = g_convert (uid->uid, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        else
            t = g_strdup (uid->uid);
                
        gtk_combo_box_append_text (combo, t);
    }
    
    gtk_combo_box_set_active (combo, 0);
}

enum 
{
    SIG_KEYID_COLUMN,
    SIG_NAME_COLUMN,
    SIG_N_COLUMNS
};

static void
add_signatures (GtkTreeStore *store, gpgme_user_id_t uid)
{
    gpgme_key_sig_t sig;
    GtkTreeIter iter;
    gchar *t;
    
    for (sig = uid->signatures; sig; sig = sig->next) {
        
        if (!g_utf8_validate (sig->uid, -1, NULL))
            t = g_convert (sig->uid, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        else
            t = g_strdup (sig->uid);
            
        gtk_tree_store_append (store, &iter, NULL);        
        gtk_tree_store_set (store, &iter, SIG_KEYID_COLUMN, sig->keyid,
                           SIG_NAME_COLUMN, t, -1);
        g_free(t);
    }
}

static void
do_signature_list (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    GtkWidget *widget;
    GtkTreeStore *store;
    GtkTreeViewColumn *column;
    gpgme_user_id_t uid;
    guint index;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    /* Get the current value from the drop down */
    widget = glade_xml_get_widget (swidget->xml, "sigs");
    index = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
    
    /* Clear/create table store */
    widget = glade_xml_get_widget (swidget->xml, "sig_list");
    store = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    
    /* This is our first time so create a store */
    if (!store) {

        store = gtk_tree_store_new (SIG_N_COLUMNS,
                                    G_TYPE_STRING, G_TYPE_STRING);
                                   
        gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));

        /* Make the column */
        column = gtk_tree_view_column_new_with_attributes (_("Key ID"), 
                                gtk_cell_renderer_text_new (), "text", SIG_KEYID_COLUMN, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
        column = gtk_tree_view_column_new_with_attributes (_("Name"), 
                                gtk_cell_renderer_text_new (), "text", SIG_NAME_COLUMN, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
        
    /* Otherwise clear it */
    } else {
        gtk_tree_store_clear (store);
    }
    
    /* All signatures */
    if (index <= 0) {
        
        for (uid = skey->key->uids; uid; uid = uid->next)
            add_signatures (store, uid);

    } else {

        index--;
        
        for (uid = skey->key->uids; uid; uid = uid->next, index--) {
            if (index == 0) {
                add_signatures (store, uid);
                break;
            }
        }            
    }        
}

static void
signature_sel_changed (GtkComboBox *optionmenu, SeahorseWidget *swidget)
{
    do_signature_list (swidget);
}

enum 
{
    UID_INDEX_COLUMN,
    UID_NAME_COLUMN,
    UID_N_COLUMNS
};

static void
uid_sel_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    GtkWidget *widget;
    gboolean selected;
    gboolean secret;
    guint uids;
    
    selected = selection != NULL && 
        gtk_tree_selection_count_selected_rows (selection) > 0;
        
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    secret = SEAHORSE_IS_KEY_PAIR (skey);
    uids = seahorse_key_get_num_uids (skey);

    widget = glade_xml_get_widget (swidget->xml, "uid-add");
    gtk_widget_set_sensitive (widget, secret);    
    widget = glade_xml_get_widget (swidget->xml, "uid-primary");
    gtk_widget_set_sensitive (widget, selected && secret && uids > 0);
    widget = glade_xml_get_widget (swidget->xml, "uid-sign");
    gtk_widget_set_sensitive (widget, selected);
    widget = glade_xml_get_widget (swidget->xml, "uid-delete");
    gtk_widget_set_sensitive (widget, selected && secret && uids > 0);
}

static void 
do_uid_list (SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    GtkWidget *widget;
    GtkTreeStore *store;
    GtkTreeViewColumn *column;
    gpgme_user_id_t uid;
    GtkTreeIter iter;
    gchar *t;
    gint i;
        
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    /* Clear/create table store */
    widget = glade_xml_get_widget (swidget->xml, "uid-list");
    store = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)));
    
    /* This is our first time so create a store */
    if (!store) {

        store = gtk_tree_store_new (UID_N_COLUMNS, 
                                    G_TYPE_INT, G_TYPE_STRING);
                                   
        gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));

        /* Make the column */
        column = gtk_tree_view_column_new_with_attributes (_("Name"), 
                        gtk_cell_renderer_text_new (), "text", UID_NAME_COLUMN, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
        
    /* Otherwise clear it */
    } else {
        gtk_tree_store_clear (store);
    }
    
    /* Add all uids. Note that uids are not zero based  */
    for (uid = skey->key->uids, i = 1; uid; uid = uid->next, i++) {
        
        if (!g_utf8_validate (uid->uid, -1, NULL))
            t = g_convert (uid->uid, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
        else
            t = g_strdup (uid->uid);
            
        gtk_tree_store_append (store, &iter, NULL);        
        gtk_tree_store_set (store, &iter, UID_INDEX_COLUMN, i, 
                           UID_NAME_COLUMN, t, -1);
        g_free (t);            

    } 
    
    uid_sel_changed (NULL, swidget);
}

static void
uid_add_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    seahorse_add_uid_new (swidget->sctx, skey);
}

static gint
get_selected_uid (SeahorseWidget *swidget)
{
    GtkTreeSelection *selection;
    GtkWidget *widget;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GList *rows;
    gint index = -1;
    
    widget = glade_xml_get_widget (swidget->xml, "uid-list");
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
    
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
    g_assert (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE);
    
    rows = gtk_tree_selection_get_selected_rows (selection, NULL);

    if (g_list_length (rows) > 0) {
        gtk_tree_model_get_iter (model, &iter, rows->data);
        gtk_tree_model_get (model, &iter, UID_INDEX_COLUMN, &index, -1);
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);                           

    return index;                           
}    

static void 
uid_primary_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    gpgme_error_t err;
    gint index;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    index = get_selected_uid (swidget);
    if (index >= 1) {
        err = seahorse_key_op_primary_uid (skey, index);
        
        if (!GPG_IS_OK (err)) 
            seahorse_util_handle_gpgme (err, _("Couldn't change primary user ID"));
    }
}

static void 
uid_sign_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    gint index;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
    
    index = get_selected_uid (swidget);
    if (index >= 1) 
       seahorse_sign_uid_show (swidget->sctx, skey, index);
}

static void 
uid_delete_clicked (GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKey *skey;
    gint index;
    
    skey = SEAHORSE_KEY_WIDGET (swidget)->skey;

    index = get_selected_uid (swidget);
    if (index >= 1) 
        seahorse_delete_userid_show (swidget->sctx, skey, index);
}
    
static void
del_subkey_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	seahorse_delete_subkey_new (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		get_subkey_index (swidget));
}

static void
revoke_subkey_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	seahorse_revoke_new (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		get_subkey_index (swidget));
}

static void
do_subkeys (SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint key, max;
	GtkTooltips *tips;
	GtkNotebook *nb;
	GtkTable *table;
	GtkWidget *widget;
	gint left = 0;
	gpgme_subkey_t subkey;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	max = seahorse_key_get_num_subkeys (skey);
	g_return_if_fail (max >= 0);
	
	tips = gtk_tooltips_new ();
	nb = GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, NOTEBOOK));
	
	/* clear subkeys */
	while (gtk_notebook_get_n_pages (nb) > 3)
		gtk_notebook_remove_page (nb, 3);
	
	/* foreach subkey */
	subkey = skey->key->subkeys;
	for (key = 1; key <= max; key++) {
		table = GTK_TABLE (gtk_table_new (5, 4, FALSE));
		/* if do revoke button */
		if (SEAHORSE_IS_KEY_PAIR (skey) && !subkey->revoked) {
			widget = do_stat_button (_("_Revoke"), GTK_STOCK_CANCEL);
			g_signal_connect_after (GTK_BUTTON (widget), "clicked",
				G_CALLBACK (revoke_subkey_clicked), swidget);
			gtk_tooltips_set_tip (tips, widget, g_strdup_printf (
				_("Revoke subkey %d"), key), NULL);
			gtk_table_attach (table, widget, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);
			left = 1;
		}
		
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		
		widget = gtk_label_new (g_strdup_printf (_("Subkey %d"), key));
		gtk_notebook_append_page (nb, GTK_WIDGET (table), widget);
		gtk_widget_show_all (GTK_WIDGET (table));
		
		do_stats (swidget, table, 0, key, subkey);
		
		/* Do delete button */
		widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
		g_signal_connect_after (GTK_BUTTON (widget), "clicked",
			G_CALLBACK (del_subkey_clicked), swidget);
		gtk_widget_show (widget);
		gtk_tooltips_set_tip (tips, widget, g_strdup_printf (
			_("Delete subkey %d"), key), NULL);
		gtk_table_attach (table, widget, left, left+1, 4, 5, GTK_FILL, 0, 0, 0);

		subkey = subkey->next;
	}
}

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseWidget *swidget)
{
	switch (change) {
        case SKEY_CHANGE_ALL:
            do_uid_list (swidget);
            key_property_labels (swidget);
            do_signatures (swidget);
            do_signature_list (swidget);
            do_subkeys (swidget);
            break;
		case SKEY_CHANGE_UIDS:
			do_uid_list (swidget);
            key_property_labels (swidget);
			break;
        case SKEY_CHANGE_SIGNERS:
            do_signatures (swidget);
            do_signature_list (swidget);
            break;
		case SKEY_CHANGE_SUBKEYS: 
        case SKEY_CHANGE_EXPIRES:
			do_subkeys (swidget);
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
seahorse_key_properties_new (SeahorseContext *sctx, SeahorseKey *skey)
{
    SeahorseKeySource *sksrc;
	SeahorseWidget *swidget;
	GtkWidget *widget;
    gboolean remote;
    
    remote = seahorse_key_get_loaded_info (skey) == SKEY_INFO_REMOTE;
    
    /* Reload the key to make sure to get all the props */
    sksrc = seahorse_key_get_source (skey);
    g_return_if_fail (sksrc != NULL);
        
    /* Don't trigger the import of remote keys if possible */
    if (!remote) {
        /* This causes the key source to get any specific info about the key */
        seahorse_key_source_refresh_sync (sksrc, seahorse_key_get_id (skey->key));
        skey = seahorse_key_source_get_key (sksrc, seahorse_key_get_id (skey->key));
        g_return_if_fail (skey != NULL);                
    }
	
	swidget = seahorse_key_widget_new ("key-properties", sctx, skey);
	g_return_if_fail (swidget != NULL);
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
    g_signal_connect_after (skey, "changed", G_CALLBACK (key_changed), swidget);
    g_signal_connect_after (skey, "destroy", G_CALLBACK (key_destroyed), swidget);
	
	key_property_labels (swidget);
    
    /* Secret key stuff */    
    if(SEAHORSE_IS_KEY_PAIR (skey)) {
        
        /* Change password button */
        widget = glade_xml_get_widget (swidget->xml, "passphrase");
        gtk_widget_set_sensitive (widget, TRUE);
        glade_xml_signal_connect_data (swidget->xml, "passphrase_clicked",
                    G_CALLBACK (passphrase_clicked), swidget);

        /* Export secret key button */
        widget = glade_xml_get_widget (swidget->xml, "export_complete");
        gtk_widget_set_sensitive (widget, TRUE);
        glade_xml_signal_connect_data (swidget->xml, "export_clicked",
                    G_CALLBACK (export_clicked), swidget);
    }

	do_subkeys (swidget);
    do_uid_list (swidget);

    /* Disable stuff not appropriate for remote */
    if (remote) {
        gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "trust"), FALSE);
        gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "disabled"), FALSE);

        /* Remove the signatures page (3rd page) */ 
        widget = glade_xml_get_widget (swidget->xml, NOTEBOOK);
        gtk_notebook_remove_page (GTK_NOTEBOOK (widget), 2);
        
    /* Stuff valid for local keys */
    } else {     

        /* Uid page */
        widget = glade_xml_get_widget (swidget->xml, "uid-list");
        g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget)), "changed",
                    G_CALLBACK (uid_sel_changed), swidget);
        glade_xml_signal_connect_data (swidget->xml, "uid_add_clicked",
                    G_CALLBACK (uid_add_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "uid_primary_clicked",
                    G_CALLBACK (uid_primary_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "uid_sign_clicked",
                    G_CALLBACK (uid_sign_clicked), swidget);
        glade_xml_signal_connect_data (swidget->xml, "uid_delete_clicked",
                    G_CALLBACK (uid_delete_clicked), swidget);
                    
        /* Signature setup */
        do_signatures (swidget);
        do_signature_list (swidget);
        glade_xml_signal_connect_data (swidget->xml, "sigs_changed",
                                       G_CALLBACK (signature_sel_changed), swidget);        

        /* disable trust options */
        if (SEAHORSE_IS_KEY_PAIR (skey))
            widget = glade_xml_get_widget (swidget->xml, "unknown");
        else
            widget = glade_xml_get_widget (swidget->xml, "ultimate");
        gtk_widget_set_sensitive (widget, FALSE);
        
    	gtk_option_menu_set_history (GTK_OPTION_MENU (glade_xml_get_widget (swidget->xml, "trust")),
	                                 seahorse_key_get_trust (skey) - 1);
        glade_xml_signal_connect_data (swidget->xml, "trust_changed",
                                       G_CALLBACK (trust_changed), swidget);
        glade_xml_signal_connect_data (swidget->xml, "disabled_toggled",
                                       G_CALLBACK (disabled_toggled), swidget);
    }
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, "disabled")),
		                          skey->key->disabled);

    seahorse_widget_show (swidget);    
}
