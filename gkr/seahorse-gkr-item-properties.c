/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include <glib/gi18n.h>

#include "seahorse-gkr-item.h"
#include "seahorse-gkr-source.h"
#include "seahorse-gkr-operation.h"

#include "seahorse-gtkstock.h"
#include "seahorse-object.h"
#include "seahorse-object-widget.h"
#include "seahorse-secure-memory.h"
#include "seahorse-secure-entry.h"
#include "seahorse-util.h"

#include "common/seahorse-bind.h"

/* -----------------------------------------------------------------------------
 * MAIN TAB 
 */

static void
update_password (SeahorseWidget *swidget, SeahorseGkrItem *git)
{
    SeahorseSecureEntry *entry;
    gchar *secret;
    
    entry = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (swidget), 
                                   "secure-password-entry"));
    if (entry) {
        
        /* Retrieve initial password. Try to keep it safe */
        WITH_SECURE_MEM ((secret = gnome_keyring_item_info_get_secret (git->info)));
        seahorse_secure_entry_set_text (entry, secret ? secret : "");
        g_free (secret);
    
        seahorse_secure_entry_reset_changed (entry);
    }
}

static gboolean
transform_item_use (const GValue *from, GValue *to)
{
	gchar *label;
	guint val;
	
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_UINT, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	
	val = g_value_get_uint (from);
	switch (val) {
	case SEAHORSE_GKR_USE_NETWORK:
		label = _("Access a network share or resource");
		break;
	case SEAHORSE_GKR_USE_WEB:
		label = _("Access a website");
		break;
	case SEAHORSE_GKR_USE_PGP:
		label = _("Unlocks a PGP key");
		break;
	case SEAHORSE_GKR_USE_SSH:
		label = _("Unlocks a Secure Shell key");
		break;
	case SEAHORSE_GKR_USE_OTHER:
		label = _("Saved password or login");
		break;
	default:
	        g_return_val_if_reached (FALSE);
	};
	
	g_value_set_string (to, label);
	return TRUE;
}

static gboolean
transform_item_type (const GValue *from, GValue *to)
{
	gchar *label;
	guint val;
	
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_UINT, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	
	val = g_value_get_uint (from);
	switch (val) {
	case SEAHORSE_GKR_USE_NETWORK:
	case SEAHORSE_GKR_USE_WEB:
		label = _("Network Credentials");
		break;
	case SEAHORSE_GKR_USE_PGP:
	case SEAHORSE_GKR_USE_SSH:
	case SEAHORSE_GKR_USE_OTHER:
		label = _("Password");
		break;
	default:
	        g_return_val_if_reached (FALSE);
	};
	
	g_value_set_string (to, label);
	return TRUE;
}

static gboolean
transform_network_visible (const GValue *from, GValue *to)
{
	gboolean result;
	guint val;
	
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_UINT, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_BOOLEAN, FALSE);
	
	val = g_value_get_uint (from);
	switch (val) {
	case SEAHORSE_GKR_USE_NETWORK:
	case SEAHORSE_GKR_USE_WEB:
		result = TRUE;
		break;
	case SEAHORSE_GKR_USE_PGP:
	case SEAHORSE_GKR_USE_SSH:
	case SEAHORSE_GKR_USE_OTHER:
		result = FALSE;
		break;
	default:
	        g_return_val_if_reached (FALSE);
	};
	
	g_value_set_boolean (to, result);
	return TRUE;
}

static gboolean
transform_attributes_server (const GValue *from, GValue *to)
{
	GnomeKeyringAttributeList *attrs;
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_POINTER, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	attrs = g_value_get_pointer (from);
	if (attrs)
		g_value_set_string (to, seahorse_gkr_find_string_attribute (attrs, "server"));
	return TRUE;
}

static gboolean
transform_attributes_user (const GValue *from, GValue *to)
{
	GnomeKeyringAttributeList *attrs;
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_POINTER, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	attrs = g_value_get_pointer (from);
	if (attrs)
		g_value_set_string (to, seahorse_gkr_find_string_attribute (attrs, "user"));
	return TRUE;
}

static void
setup_main (SeahorseWidget *swidget)
{
	SeahorseObject *object;
	
	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;

	/* Setup the image properly */
	seahorse_bind_property ("icon", object, "stock", 
	                        seahorse_widget_get_widget (swidget, "key-image"));
	
	/* Setup the label properly */
	seahorse_bind_property ("label", object, "text", 
	                        seahorse_widget_get_widget (swidget, "description-field") );
	
	/* Window title */
	seahorse_bind_property ("label", object, "title", 
	                        seahorse_widget_get_toplevel (swidget));
	
	/* Usage */
	seahorse_bind_property_full ("use", object, transform_item_use, "label", 
	                             seahorse_widget_get_widget (swidget, "use-field"), NULL);
	
	/* Item Type */
	seahorse_bind_property_full ("use", object, transform_item_type, "label", 
	                             seahorse_widget_get_widget (swidget, "type-field"), NULL);
	
	/* Network field visibility */
	seahorse_bind_property_full ("use", object, transform_network_visible, "visible",
	                             seahorse_widget_get_widget (swidget, "server-label"),
	                             seahorse_widget_get_widget (swidget, "server-field"),
	                             seahorse_widget_get_widget (swidget, "login-label"),
	                             seahorse_widget_get_widget (swidget, "login-field"), NULL);

	/* Server name */
	seahorse_bind_property_full ("item-attributes", object, transform_attributes_server, "label", 
	                             seahorse_widget_get_widget (swidget, "server-field"), NULL);
	
	/* User name */
	seahorse_bind_property_full ("item-attributes", object, transform_attributes_user, "label", 
	                             seahorse_widget_get_widget (swidget, "login-field"), NULL);
}

static void
password_activate (SeahorseSecureEntry *entry, SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseGkrItem *git;
    SeahorseOperation *op;
    GnomeKeyringItemInfo *info;
    GtkWidget *widget;
    GError *err = NULL;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    if (!object)
	    return;
    git = SEAHORSE_GKR_ITEM (object);

    widget = seahorse_widget_get_widget (swidget, "password-expander");
    g_return_if_fail (widget);
    if (!gtk_expander_get_expanded (GTK_EXPANDER (widget)))
        return;

    entry = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (swidget), 
                                                "secure-password-entry"));
    if (!entry || !seahorse_secure_entry_get_changed (entry))
        return;
        
    /* Setup for saving */
    WITH_SECURE_MEM (info = gnome_keyring_item_info_copy (git->info));
    WITH_SECURE_MEM (gnome_keyring_item_info_set_secret (info, 
                            seahorse_secure_entry_get_text (entry)));

    gtk_widget_set_sensitive (GTK_WIDGET (entry), FALSE);
    
    op = seahorse_gkr_operation_update_info (git, info);
    gnome_keyring_item_info_free (info);
    
    /* This is usually a quick operation */
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't change password."));
        g_clear_error (&err);
        update_password (swidget, git);
    }
    
    gtk_widget_set_sensitive (GTK_WIDGET (entry), TRUE);
}

static gboolean
password_focus_out (SeahorseSecureEntry* entry, GdkEventFocus *event, SeahorseWidget *swidget)
{
    password_activate (entry, swidget);
    return FALSE;
}

static void 
show_password_toggled (GtkToggleButton *button, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    
    widget = GTK_WIDGET (g_object_get_data (G_OBJECT (swidget), "secure-password-entry"));
    if (!widget)
        return;
    
    seahorse_secure_entry_set_visibility (SEAHORSE_SECURE_ENTRY (widget), 
                                          gtk_toggle_button_get_active (button));
}

static void
password_expander_activate (GtkExpander *expander, SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseGkrItem *git;
    GtkWidget *widget;
    GtkWidget *box;

    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    git = SEAHORSE_GKR_ITEM (object);

    if (!gtk_expander_get_expanded (expander))
        return;

    widget = GTK_WIDGET (g_object_get_data (G_OBJECT (swidget), "secure-password-entry"));
    if (!widget) {
        widget = seahorse_secure_entry_new ();
        
        box = seahorse_widget_get_widget (swidget, "password-box-area");
        g_return_if_fail (box != NULL);
        gtk_container_add (GTK_CONTAINER (box), widget);
        g_object_set_data (G_OBJECT (swidget), "secure-password-entry", widget);
        gtk_widget_show (widget);
        
        /* Retrieve initial password */
        update_password (swidget, git);
        
        /* Now watch for changes in the password */
        g_signal_connect (widget, "activate", G_CALLBACK (password_activate), swidget);
        g_signal_connect_after (widget, "focus-out-event", G_CALLBACK (password_focus_out), swidget);
    }
    
    /* Always have a hidden password when opening box */
    widget = seahorse_widget_get_widget (swidget, "show-password-check");
    g_return_if_fail (widget != NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
}

static void
description_activate (GtkWidget *entry, SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseGkrItem *git;
    SeahorseOperation *op;
    GnomeKeyringItemInfo *info;
    const gchar *text;
    gchar *original;
    GError *err = NULL;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    if (!object)
	    return;
    
    git = SEAHORSE_GKR_ITEM (object);

    text = gtk_entry_get_text (GTK_ENTRY (entry));
    original = seahorse_gkr_item_get_description (git);
    
    /* Make sure not the same */
    if (text == original || g_utf8_collate (text, original ? original : "") == 0) {
        g_free (original);
        return;
    }

    gtk_widget_set_sensitive (entry, FALSE);
    
    WITH_SECURE_MEM (info = gnome_keyring_item_info_copy (git->info));
    gnome_keyring_item_info_set_display_name (info, text);
    
    op = seahorse_gkr_operation_update_info (git, info);
    gnome_keyring_item_info_free (info);
    
    /* This is usually a quick operation */
    seahorse_operation_wait (op);
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't set description."));
        g_clear_error (&err);
        gtk_entry_set_text (GTK_ENTRY (entry), original);
    }
    
    gtk_widget_set_sensitive (entry, TRUE);
    
    g_free (original);
}

static gboolean
description_focus_out (GtkWidget* widget, GdkEventFocus *event, SeahorseWidget *swidget)
{
	description_activate (widget, swidget);
	return FALSE;
}

/* -----------------------------------------------------------------------------
 * DETAILS TAB
 */

static gboolean
transform_item_details (const GValue *from, GValue *to)
{
	GnomeKeyringAttributeList *attrs;
	GnomeKeyringAttribute *attr;
	GString *details;
	guint i;

	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_POINTER, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	attrs = g_value_get_pointer (from);
	
	details = g_string_new (NULL);
	if (attrs) {
		/* Build up the display string */
		for(i = 0; i < attrs->len; ++i) {
			attr = &(gnome_keyring_attribute_list_index (attrs, i));
			g_string_append_printf (details, "<b>%s</b>: ", attr->name);
			switch (attr->type) {
			case GNOME_KEYRING_ATTRIBUTE_TYPE_STRING:
				g_string_append_printf (details, "%s\n", attr->value.string);
				break;
			case GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32:
				g_string_append_printf (details, "%u\n", attr->value.integer);
				break;
			default:
				g_string_append (details, "<i>[invalid]</i>\n");
				break;
			}
		}
	}
	    
	g_value_take_string (to, g_string_free (details, FALSE));
	return TRUE;
}

static void 
setup_details (SeahorseWidget *swidget)
{
	SeahorseObject *object;
	GtkWidget *widget;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    
	widget = seahorse_widget_get_widget (swidget, "details-box");
	g_return_if_fail (widget != NULL);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
    
	seahorse_bind_property_full ("item-attributes", object, transform_item_details, 
	                             "label", widget, NULL);
}

/* -----------------------------------------------------------------------------
 * APPLICATIONS TAB 
 */

static gint
selected_application_index (SeahorseWidget *swidget)
{
    GtkTreeView *tree;
    GtkTreePath *path;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    gint* indices;
    gint ret;
    
    tree = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "application-list"));
    g_return_val_if_fail (GTK_IS_TREE_VIEW (tree), -1);
    
    selection = gtk_tree_view_get_selection (tree);
    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return -1;
        
    path = gtk_tree_model_get_path (model, &iter);
    g_return_val_if_fail (path, -1);
    
    indices = gtk_tree_path_get_indices (path);
    g_return_val_if_fail (indices, -1);
    
    ret = *indices;
    gtk_tree_path_free (path);
    
    return ret;
}

static void
update_application_details (SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseGkrItem *git;
    GnomeKeyringAccessControl *ac;
    GtkLabel *label;
    GtkToggleButton *toggle;
    GnomeKeyringAccessType access;
    gint index;
    gchar *path;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    git = SEAHORSE_GKR_ITEM (object);

    index = selected_application_index (swidget);
    if (index < 0) {
        ac = NULL;
    } else {
        ac = (GnomeKeyringAccessControl*)g_list_nth_data (git->acl, index);
        g_return_if_fail (ac);
    }
        
    seahorse_widget_set_sensitive (swidget, "application-details", ac != NULL);
    
    label = GTK_LABEL (seahorse_widget_get_widget (swidget, "application-path"));
    g_return_if_fail (GTK_IS_LABEL (label));
    path = ac ? gnome_keyring_item_ac_get_path_name (ac) : NULL;
    gtk_label_set_text (label, path ? path : "");
    g_free (path);

    g_object_set_data (G_OBJECT (swidget), "updating", "updating");
    access = ac ? gnome_keyring_item_ac_get_access_type (ac) : 0;
    
    toggle = GTK_TOGGLE_BUTTON (seahorse_widget_get_widget (swidget, "application-read"));
    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle));
    gtk_toggle_button_set_active (toggle, access & GNOME_KEYRING_ACCESS_READ);    

    toggle = GTK_TOGGLE_BUTTON (seahorse_widget_get_widget (swidget, "application-write"));
    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle));    
    gtk_toggle_button_set_active (toggle, access & GNOME_KEYRING_ACCESS_WRITE);    

    toggle = GTK_TOGGLE_BUTTON (seahorse_widget_get_widget (swidget, "application-delete"));
    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle));
    gtk_toggle_button_set_active (toggle, access & GNOME_KEYRING_ACCESS_REMOVE);
    
    g_object_set_data (G_OBJECT (swidget), "updating", NULL);
}

static void
application_selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
    update_application_details (swidget);
}

static void 
merge_toggle_button_access (SeahorseWidget *swidget, const gchar *identifier, 
                            GnomeKeyringAccessType *access, GnomeKeyringAccessType type)
{
    GtkToggleButton *toggle;
    
    toggle = GTK_TOGGLE_BUTTON (seahorse_widget_get_widget (swidget, identifier));
    g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle));
    if (gtk_toggle_button_get_active (toggle))
        *access |= type;
    else
        *access &= ~type;
}

static void
application_access_toggled (GtkCheckButton *check, SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseGkrItem *git;
    SeahorseOperation *op;
    GnomeKeyringAccessType access;
    GnomeKeyringAccessControl *ac;
    GError *err = NULL;
    GList *acl;
    guint index;
    
    /* Just us setting up the controls, not the user */
    if (g_object_get_data (G_OBJECT (swidget), "updating"))
        return;
    
    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    git = SEAHORSE_GKR_ITEM (object);

    index = selected_application_index (swidget);
    g_return_if_fail (index >= 0);
    
    acl = gnome_keyring_acl_copy (git->acl);
    ac = (GnomeKeyringAccessControl*)g_list_nth_data (acl, index);
    g_return_if_fail (ac);
    
    access = gnome_keyring_item_ac_get_access_type (ac);
    
    merge_toggle_button_access (swidget, "application-read", &access, GNOME_KEYRING_ACCESS_READ);    
    merge_toggle_button_access (swidget, "application-write", &access, GNOME_KEYRING_ACCESS_WRITE);    
    merge_toggle_button_access (swidget, "application-delete", &access, GNOME_KEYRING_ACCESS_REMOVE);    

    if (access != gnome_keyring_item_ac_get_access_type (ac)) {
        
        gnome_keyring_item_ac_set_access_type (ac, access);

        seahorse_widget_set_sensitive (swidget, "application-details", FALSE);
        
        op = seahorse_gkr_operation_update_acl (git, acl);
        g_return_if_fail (op);
        
        seahorse_operation_wait (op);
        if (!seahorse_operation_is_successful (op)) {
            seahorse_operation_copy_error (op, &err);
            seahorse_util_handle_error (err, _("Couldn't set application access."));
            g_clear_error (&err);
            update_application_details (swidget);
        }
                
        seahorse_widget_set_sensitive (swidget, "application-details", TRUE);        
    }
    
    gnome_keyring_acl_free (acl);
}

static void 
update_application (SeahorseGkrItem *git, SeahorseWidget *swidget)
{
    GtkTreeView *tree;
    GtkListStore *store;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkCellRenderer *renderer;
    GnomeKeyringAccessControl *ac;
    GtkTreeViewColumn *column;
    GList *acl;
    gboolean valid;
    gchar *display;

    tree = GTK_TREE_VIEW (seahorse_widget_get_widget (swidget, "application-list"));
    g_return_if_fail (tree);
    
    model = gtk_tree_view_get_model (tree);
    if (!model) {
        store = gtk_list_store_new (1, GTK_TYPE_STRING);
        model = GTK_TREE_MODEL (store);
        gtk_tree_view_set_model (tree, model);
        
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("name", renderer, "text", 0, NULL);
        gtk_tree_view_append_column (tree, column);
    } else {
        store = GTK_LIST_STORE (model);
    }
    
    acl = git->acl;
    
    /* Fill in the tree store, replacing any rows already present */
    valid = gtk_tree_model_get_iter_first (model, &iter);
    for ( ; acl; acl = g_list_next (acl)) {
    
        ac = (GnomeKeyringAccessControl*)acl->data;
        g_return_if_fail (ac);
        
        if (!valid)
            gtk_list_store_append (store, &iter);

        display = gnome_keyring_item_ac_get_display_name (ac);
        gtk_list_store_set (store, &iter, 0, display ? display : "", -1);
        g_free (display);
        
        if (valid)
            valid = gtk_tree_model_iter_next (model, &iter);
    }
    
    /* Remove all the remaining rows */
    while (valid)
        valid = gtk_list_store_remove (store, &iter);
        
    update_application_details (swidget);
}

static void 
setup_application (SeahorseWidget *swidget)
{
	seahorse_bind_objects ("item-acl", SEAHORSE_OBJECT_WIDGET (swidget)->object,
	                       (SeahorseTransfer)update_application, swidget);
}

/* -----------------------------------------------------------------------------
 * GENERAL
 */

static void
properties_response (GtkDialog *dialog, int response, SeahorseWidget *swidget)
{
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help(swidget);
        return;
    }
   
    seahorse_widget_destroy (swidget);
}

void
seahorse_gkr_item_properties_show (SeahorseGkrItem *git, GtkWindow *parent)
{
    SeahorseObject *object = SEAHORSE_OBJECT (git);
    SeahorseSource *sksrc;
    SeahorseWidget *swidget = NULL;
    GtkWidget *widget;

    swidget = seahorse_object_widget_new ("gkr-item-properties", parent, object);
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return;

    /* This causes the key source to get any specific info about the key */
    sksrc = seahorse_object_get_source (SEAHORSE_OBJECT (git));
    seahorse_source_load_async (sksrc, seahorse_object_get_id (object));

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    setup_main (swidget);
    setup_details (swidget);
    setup_application (swidget);
    
    widget = seahorse_widget_get_widget (swidget, "password-expander");
    g_return_if_fail (widget);
    g_signal_connect_after (widget, "activate", G_CALLBACK (password_expander_activate), swidget);

    glade_xml_signal_connect_data (swidget->xml, "show_password_toggled", 
                                   G_CALLBACK (show_password_toggled), swidget);

    widget = seahorse_widget_get_widget (swidget, "description-field");
    g_return_if_fail (widget != NULL);
    g_signal_connect (widget, "activate", G_CALLBACK (description_activate), swidget);
    g_signal_connect (widget, "focus-out-event", G_CALLBACK (description_focus_out), swidget);

    glade_xml_signal_connect_data (swidget->xml, "application_access_toggled", 
                                   G_CALLBACK (application_access_toggled), swidget);

    widget = seahorse_widget_get_widget (swidget, "application-list");
    g_return_if_fail (GTK_IS_TREE_VIEW (widget));
    
    g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget)), "changed", 
                      G_CALLBACK (application_selection_changed), swidget);
}
