/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Nate Nielsen
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

enum {
	PROP_0,
    PROP_NONE_OPTION,
    PROP_KEYSET
};

G_DEFINE_TYPE (SeahorseDefaultKeyControl, seahorse_default_key_control, GTK_TYPE_OPTION_MENU);

static void    seahorse_default_key_control_finalize        (GObject *gobject);
static void    seahorse_default_key_control_set_property    (GObject *object, guint prop_id,
                                                             const GValue *value, GParamSpec *pspec);
static void    seahorse_default_key_control_get_property    (GObject *object, guint prop_id,
                                                             GValue *value, GParamSpec *pspec);

static GtkOptionMenuClass *parent_class = NULL;

static void
seahorse_default_key_control_class_init (SeahorseDefaultKeyControlClass *klass)
{
    GObjectClass *gobject_class;
    
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->finalize = seahorse_default_key_control_finalize;
    gobject_class->set_property = seahorse_default_key_control_set_property;
    gobject_class->get_property = seahorse_default_key_control_get_property;
        
    g_object_class_install_property (gobject_class, PROP_KEYSET,
            g_param_spec_object ("keyset", "Key Set", "Set of keys to pic from",
                                 SEAHORSE_TYPE_KEYSET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_NONE_OPTION,
            g_param_spec_string ("none-option", "No key option", "Puts in an option for 'no key'",
                                  NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
key_added (SeahorseKeyset *skset, SeahorseKey *skey, SeahorseDefaultKeyControl *sdkc)
{
    GtkWidget *menu;
    GtkWidget *item;
    gchar *userid;
    
    menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (sdkc));
    
    userid = seahorse_key_get_display_name (skey);
    item = gtk_menu_item_new_with_label (userid);
    g_free (userid);

    g_object_set_data (G_OBJECT (item), "key", skey);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show (item);
    
    seahorse_keyset_set_closure (skset, skey, item);
}

static void
key_removed (SeahorseKeyset *skset, SeahorseKey *skey, GtkWidget *item, 
             SeahorseDefaultKeyControl *sdkc)
{
    gtk_widget_destroy (item);
}

/* TODO: We should be handling key changed and displaying the right display name */

static void    
seahorse_default_key_control_init (SeahorseDefaultKeyControl *sdkc)
{
    GtkWidget *menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (sdkc), menu);
}

static void
seahorse_default_key_control_finalize (GObject *gobject)
{
    g_signal_handlers_disconnect_by_func (SCTX_APP (), key_added, GTK_WIDGET (gobject));
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_default_key_control_set_property (GObject *object, guint prop_id,
                                           const GValue *value, GParamSpec *pspec)
{
    SeahorseDefaultKeyControl *control;
    SeahorseKey *skey;
    GList *l, *keys;
    const char *t;
    
    control = SEAHORSE_DEFAULT_KEY_CONTROL (object);
    
    switch (prop_id) {
    case PROP_KEYSET:
        g_return_if_fail (control->skset == NULL);        
        control->skset = g_value_get_object (value);

        keys = seahorse_keyset_get_keys (control->skset);  
        for (l = keys; l != NULL; l = g_list_next (l)) {
            skey = SEAHORSE_KEY (l->data);
            key_added (control->skset, skey, control);
        }
     
        g_list_free (keys);

        g_signal_connect_after (SCTX_APP (), "added", G_CALLBACK (key_added), GTK_WIDGET (control));
        g_signal_connect_after (SCTX_APP (), "removed", G_CALLBACK (key_removed), GTK_WIDGET (control));
        break;

    case PROP_NONE_OPTION:
        if ((t = g_value_get_string (value)) != NULL) {
            GtkMenu *menu;
            GtkWidget *item;
            
            menu = GTK_MENU (gtk_option_menu_get_menu (GTK_OPTION_MENU (control)));
            g_return_if_fail (menu != NULL);
        
            item = gtk_separator_menu_item_new ();
            gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
            gtk_widget_show (item);
            
            item = gtk_menu_item_new_with_label (t);
            gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
            gtk_widget_show (item);

            gtk_option_menu_set_history (GTK_OPTION_MENU (control), 0);
            g_object_set_data (object, "none-option", GINT_TO_POINTER (1));
        }
        break;
    }
}

static void
seahorse_default_key_control_get_property (GObject *object, guint prop_id,
                                           GValue *value, GParamSpec *pspec)
{
    SeahorseDefaultKeyControl *control = SEAHORSE_DEFAULT_KEY_CONTROL (object);
    
    switch (prop_id) {
        case PROP_KEYSET:
            g_value_set_object (value, control->skset);
            break;
        case PROP_NONE_OPTION:
            g_value_set_boolean (value, g_object_get_data (object, "none-option") ?
                                                TRUE : FALSE);
            break;
    }
}

SeahorseDefaultKeyControl*  
seahorse_default_key_control_new (SeahorseKeyset *skset, const gchar *none_option)
{
    return g_object_new (SEAHORSE_TYPE_DEFAULT_KEY_CONTROL, "keyset", skset, 
                         "none-option", none_option, NULL);
}

void                        
seahorse_default_key_control_select (SeahorseDefaultKeyControl *sdkc, 
                                     SeahorseKey *skey)
{
    seahorse_default_key_control_select_id (sdkc, 
            skey == NULL ? NULL : seahorse_key_get_keyid (skey));
}    
    
void                        
seahorse_default_key_control_select_id (SeahorseDefaultKeyControl *sdkc, 
                                        const gchar *id)
{
    SeahorseKey *skey;
    GtkContainer *menu;
    GList *l, *children;
    const gchar *x;
    guint i;
    
    /* Zero length string is null */
    if (id && !id[0])
        id = NULL;
    
    g_return_if_fail (SEAHORSE_IS_DEFAULT_KEY_CONTROL (sdkc));

    menu = GTK_CONTAINER (gtk_option_menu_get_menu (GTK_OPTION_MENU (sdkc)));
    g_return_if_fail (menu != NULL);
    
    children = gtk_container_get_children (menu);
    
    for (i = 0, l = children; l != NULL; i++, l = g_list_next (l)) {
        skey = SEAHORSE_KEY (g_object_get_data (l->data, "key"));
        
        if (id == NULL) {
            if (skey == NULL) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (sdkc), i);
                break;
            }
        } else if (skey != NULL) {
            x = seahorse_key_get_keyid (skey);
            if (x != NULL && g_str_equal (x, id)) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (sdkc), i);
                break;
            }
        }
    }

    g_list_free (children);
}

SeahorseKey*
seahorse_default_key_control_active (SeahorseDefaultKeyControl *sdkc)
{
    SeahorseKey *secret = NULL;
    GtkContainer *menu;
    GList *l, *children;
    guint i;
    
    g_return_val_if_fail (SEAHORSE_IS_DEFAULT_KEY_CONTROL (sdkc), NULL);

    menu = GTK_CONTAINER (gtk_option_menu_get_menu (GTK_OPTION_MENU (sdkc)));
    g_return_val_if_fail (menu != NULL, NULL);
    
    children = gtk_container_get_children (menu);
    
    for (i = 0, l = children; l != NULL; i++, l = g_list_next (l)) {
        if (i == gtk_option_menu_get_history (GTK_OPTION_MENU (sdkc))) {
           secret = SEAHORSE_KEY (g_object_get_data (l->data, "key"));
           break;
        }
    }

    g_list_free (children);
    return secret;
}

const gchar*                
seahorse_default_key_control_active_id (SeahorseDefaultKeyControl *sdkc)
{
    SeahorseKey *skey = seahorse_default_key_control_active (sdkc);
    return skey == NULL ? NULL : seahorse_key_get_keyid (skey);
}


