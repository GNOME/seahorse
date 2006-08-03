/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Nate Nielsen
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

#include "seahorse-default-key-control.h"
#include "seahorse-key.h"
#include "seahorse-gconf.h"

/* TODO: This file should be renamed to seahorse-combo-keys.c once we're using SVN */

/* -----------------------------------------------------------------------------
 * HELPERS 
 */

static void
key_added (SeahorseKeyset *skset, SeahorseKey *skey, GtkOptionMenu *combo)
{
    GtkWidget *menu;
    GtkWidget *item;
    gchar *userid;
    
    g_return_if_fail (skey != NULL);
    g_return_if_fail (combo != NULL);
    
    menu = gtk_option_menu_get_menu (combo);
    
    userid = seahorse_key_get_display_name (skey);
    item = gtk_menu_item_new_with_label (userid);
    g_free (userid);

    g_object_set_data (G_OBJECT (item), "key", skey);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show (item);
    
    seahorse_keyset_set_closure (skset, skey, item);
}

static void
key_changed (SeahorseKeyset *skset, SeahorseKey *skey, SeahorseKeyChange change, 
             GtkWidget *item, GtkOptionMenu *combo)
{
    GList *children;
    gchar *userid;
    
    g_return_if_fail (skey != NULL);
    g_return_if_fail (item != NULL);

    children = gtk_container_get_children (GTK_CONTAINER (item));
    
    if (children && GTK_IS_LABEL (children->data)) {
        userid = seahorse_key_get_display_name (skey);
        gtk_label_set_text (GTK_LABEL (children->data), userid);
        g_free (userid);
    }
    
    g_list_free (children);
}

static void
key_removed (SeahorseKeyset *skset, SeahorseKey *skey, GtkWidget *item, 
             GtkOptionMenu *combo)
{
    g_return_if_fail (skey != NULL);
    g_return_if_fail (item != NULL);

    gtk_widget_destroy (item);
}

static void
combo_destroyed (GtkOptionMenu *combo, SeahorseKeyset *skset)
{
    g_signal_handlers_disconnect_by_func (skset, key_added, combo);
    g_signal_handlers_disconnect_by_func (skset, key_changed, combo);
    g_signal_handlers_disconnect_by_func (skset, key_removed, combo);
}

/* -----------------------------------------------------------------------------
 * PUBLIC CALLS
 */

void 
seahorse_combo_keys_attach (GtkOptionMenu *combo, SeahorseKeyset *skset,
                            const gchar *none_option)
{
    GtkMenu *menu;
    GtkWidget *item;
    SeahorseKey *skey;
    GList *l, *keys;

    /* Setup the None Option */
    menu = GTK_MENU (gtk_option_menu_get_menu (combo));
    if (!menu) {
        menu = GTK_MENU (gtk_menu_new ());
        gtk_option_menu_set_menu (combo, GTK_WIDGET (menu));
    }

    /* Setup the key list */
    keys = seahorse_keyset_get_keys (skset);  
    for (l = keys; l != NULL; l = g_list_next (l)) {
        skey = SEAHORSE_KEY (l->data);
        key_added (skset, skey, combo);
    }
    g_list_free (keys);

    g_signal_connect_after (skset, "added", G_CALLBACK (key_added), combo);
    g_signal_connect_after (skset, "changed", G_CALLBACK (key_changed), combo);
    g_signal_connect_after (skset, "removed", G_CALLBACK (key_removed), combo);

    if (none_option) {
        item = gtk_separator_menu_item_new ();
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);
        
        item = gtk_menu_item_new_with_label (none_option);
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);
    }

    gtk_option_menu_set_history (combo, 0);
    
    /* Cleanup */
    g_object_ref (skset);
    g_object_set_data_full (G_OBJECT (combo), "skset", skset, g_object_unref);
    g_signal_connect (combo, "destroy", G_CALLBACK (combo_destroyed), skset);
}

void
seahorse_combo_keys_set_active_id (GtkOptionMenu *combo, GQuark keyid)
{
    SeahorseKey *skey;
    GtkContainer *menu;
    GList *l, *children;
    guint i;
    
    g_return_if_fail (GTK_IS_OPTION_MENU (combo));

    menu = GTK_CONTAINER (gtk_option_menu_get_menu (combo));
    g_return_if_fail (menu != NULL);
    
    children = gtk_container_get_children (menu);
    
    for (i = 0, l = children; l != NULL; i++, l = g_list_next (l)) {
        skey = SEAHORSE_KEY (g_object_get_data (l->data, "key"));
        
        if (!keyid) {
            if (!skey) {
                gtk_option_menu_set_history (combo, i);
                break;
            }
        } else if (skey != NULL) {
            if (keyid == seahorse_key_get_keyid (skey)) {
                gtk_option_menu_set_history (combo, i);
                break;
            }
        }
    }

    g_list_free (children);
}

void 
seahorse_combo_keys_set_active (GtkOptionMenu *combo, SeahorseKey *skey)
{
    seahorse_combo_keys_set_active_id (combo, 
                skey == NULL ? 0 : seahorse_key_get_keyid (skey));
}

SeahorseKey* 
seahorse_combo_keys_get_active (GtkOptionMenu *combo)
{
    SeahorseKey *skey = NULL;
    GtkContainer *menu;
    GList *l, *children;
    guint i;
    
    g_return_val_if_fail (GTK_IS_OPTION_MENU (combo), NULL);
    
    menu = GTK_CONTAINER (gtk_option_menu_get_menu (combo));
    g_return_val_if_fail (menu != NULL, NULL);
    
    children = gtk_container_get_children (menu);
    
    for (i = 0, l = children; l != NULL; i++, l = g_list_next (l)) {
        if (i == gtk_option_menu_get_history (combo)) {
           skey = SEAHORSE_KEY (g_object_get_data (l->data, "key"));
           break;
        }
    }

    g_list_free (children);
    return skey;
}

GQuark 
seahorse_combo_keys_get_active_id (GtkOptionMenu *combo)
{
    SeahorseKey *skey = seahorse_combo_keys_get_active (combo);
    return skey == NULL ? 0 : seahorse_key_get_keyid (skey);
}
