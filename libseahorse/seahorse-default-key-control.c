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
#include "seahorse-key-pair.h"
#include "seahorse-gconf.h"

enum {
	PROP_0,
    PROP_NONE_OPTION,
    PROP_KEY_SOURCE
};

static void    seahorse_default_key_control_class_init      (SeahorseDefaultKeyControlClass    *klass);
static void    seahorse_default_key_control_init            (SeahorseDefaultKeyControl *sdkc);
static void    seahorse_default_key_control_finalize        (GObject                *gobject);
static void    seahorse_default_key_control_set_property    (GObject                *object,
                                                             guint                    prop_id,
                                                             const GValue                *value,
                                                             GParamSpec                *pspec);
static void    seahorse_default_key_control_get_property    (GObject                *object,
                                                             guint                    prop_id,
                                                             GValue                    *value,
                                                             GParamSpec                *pspec);

static GtkOptionMenuClass *parent_class = NULL;

GType
seahorse_default_key_control_get_type (void)
{
    static GType control_type = 0;
    
    if (!control_type) {
        static const GTypeInfo control_info = {
            sizeof (SeahorseDefaultKeyControlClass), NULL, NULL,
            (GClassInitFunc) seahorse_default_key_control_class_init,
            NULL, NULL, sizeof (SeahorseDefaultKeyControl), 0, (GInstanceInitFunc) seahorse_default_key_control_init
        };
        
        control_type = g_type_register_static (GTK_TYPE_OPTION_MENU, "SeahorseDefaultKeyControl", &control_info, 0);
    }
    
    return control_type;
}

static void
seahorse_default_key_control_class_init (SeahorseDefaultKeyControlClass *klass)
{
    GObjectClass *gobject_class;
    
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->finalize = seahorse_default_key_control_finalize;
    gobject_class->set_property = seahorse_default_key_control_set_property;
    gobject_class->get_property = seahorse_default_key_control_get_property;
        
    g_object_class_install_property (gobject_class, PROP_KEY_SOURCE,
            g_param_spec_object ("key-source", "Key Source", "Key Source to pull keys from",
                                 SEAHORSE_TYPE_KEY_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_NONE_OPTION,
            g_param_spec_string ("none-option", "No key option", "Puts in an option for 'no key'",
                                  NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
key_destroyed (GtkObject *object, GtkWidget *widget)
{
    gtk_widget_destroy (GTK_WIDGET (widget));
}

static void 
item_destroyed (GtkObject *object, gpointer data)
{
    g_signal_handlers_disconnect_by_func (SEAHORSE_KEY_PAIR (data), 
                key_destroyed, GTK_MENU_ITEM (object));
}

static void
key_added (SeahorseKeySource *sksrc, SeahorseKey *skey, SeahorseDefaultKeyControl *sdkc)
{
    GtkWidget *menu;
    GtkWidget *item;
    gchar *userid;
    
    if (!SEAHORSE_IS_KEY_PAIR (skey) || !seahorse_key_pair_can_sign (SEAHORSE_KEY_PAIR (skey)))
        return;
    
    menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (sdkc));
    
    userid = seahorse_key_get_userid (skey, 0);
    item = gtk_menu_item_new_with_label (userid);
    g_free (userid);

    g_object_set_data (G_OBJECT (item), "secret-key", SEAHORSE_KEY_PAIR (skey));

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show (item);
    
    g_signal_connect_after (GTK_OBJECT (skey), "destroy",
        G_CALLBACK (key_destroyed), item);
    g_signal_connect_after (GTK_MENU_ITEM (item), "destroy", 
        G_CALLBACK (item_destroyed), skey); 
}

static void    
seahorse_default_key_control_init (SeahorseDefaultKeyControl *sdkc)
{
    GtkWidget *menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (sdkc), menu);

    sdkc->sksrc = NULL;
}

static void
seahorse_default_key_control_finalize (GObject *gobject)
{
    SeahorseDefaultKeyControl *control;
    
    control = SEAHORSE_DEFAULT_KEY_CONTROL (gobject);

    if (control->sksrc) {
        g_object_unref (control->sksrc);
        g_signal_handlers_disconnect_by_func (control->sksrc, key_added, GTK_WIDGET (gobject));
    }
    
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
    case PROP_KEY_SOURCE:
        g_return_if_fail (control->sksrc == NULL);
        control->sksrc = g_value_get_object (value);
        g_object_ref (control->sksrc);
    
        keys = seahorse_key_source_get_keys (control->sksrc, TRUE);
        
        for (l = keys; l != NULL; l = g_list_next (l)) {
            skey = SEAHORSE_KEY (l->data);
            key_added (control->sksrc, skey, control);
        }
     
        g_list_free (keys);

        g_signal_connect_after (control->sksrc, "added", G_CALLBACK (key_added), control);
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
        
    default:
        break;
    }
}

static void
seahorse_default_key_control_get_property (GObject *object, guint prop_id,
                                           GValue *value, GParamSpec *pspec)
{
    SeahorseDefaultKeyControl *control;
    
    control = SEAHORSE_DEFAULT_KEY_CONTROL (object);
    
    switch (prop_id) {
        case PROP_KEY_SOURCE:
            g_value_set_object (value, control->sksrc);
            break;

        case PROP_NONE_OPTION:
            g_value_set_boolean (value, g_object_get_data (object, "none-option") ?
                                                TRUE : FALSE);
            break;
        
        default:
            break;
    }
}

SeahorseDefaultKeyControl*  
seahorse_default_key_control_new (SeahorseKeySource *sksrc, const gchar *none_option)
{
    return g_object_new (SEAHORSE_TYPE_DEFAULT_KEY_CONTROL, "key-source", sksrc, 
                         "none-option", none_option, NULL);
}

void                        
seahorse_default_key_control_select (SeahorseDefaultKeyControl *sdkc, 
                                     SeahorseKeyPair *skeypair)
{
    seahorse_default_key_control_select_id (sdkc, 
            skeypair == NULL ? NULL : seahorse_key_pair_get_id (skeypair));
}    
    
void                        
seahorse_default_key_control_select_id (SeahorseDefaultKeyControl *sdkc, 
                                        const gchar *id)
{
    SeahorseKeyPair *skp;
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
        skp = SEAHORSE_KEY_PAIR (g_object_get_data (l->data, "secret-key"));
        
        if (id == NULL) {
            if (skp == NULL) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (sdkc), i);
                break;
            }
        } else if (skp != NULL) {
            x = seahorse_key_pair_get_id (skp);
            if (x != NULL && g_str_equal (x, id)) {
                gtk_option_menu_set_history (GTK_OPTION_MENU (sdkc), i);
                break;
            }
        }
    }

    g_list_free (children);
}

SeahorseKeyPair*
seahorse_default_key_control_active (SeahorseDefaultKeyControl *sdkc)
{
    SeahorseKeyPair *secret = NULL;
    GtkContainer *menu;
    GList *l, *children;
    guint i;
    
    g_return_val_if_fail (SEAHORSE_IS_DEFAULT_KEY_CONTROL (sdkc), NULL);

    menu = GTK_CONTAINER (gtk_option_menu_get_menu (GTK_OPTION_MENU (sdkc)));
    g_return_val_if_fail (menu != NULL, NULL);
    
    children = gtk_container_get_children (menu);
    
    for (i = 0, l = children; l != NULL; i++, l = g_list_next (l)) {
        if (i == gtk_option_menu_get_history (GTK_OPTION_MENU (sdkc))) {
           secret = SEAHORSE_KEY_PAIR (g_object_get_data (l->data, "secret-key"));
           break;
        }
    }

    g_list_free (children);
    return secret;
}

const gchar*                
seahorse_default_key_control_active_id (SeahorseDefaultKeyControl *sdkc)
{
    SeahorseKeyPair *skp = seahorse_default_key_control_active (sdkc);
    return skp == NULL ? NULL : seahorse_key_pair_get_id (skp);
}


