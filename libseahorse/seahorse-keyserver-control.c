/*
 * Seahorse
 *
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

#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-keyserver-control.h"
#include "seahorse-key.h"
#include "seahorse-gconf.h"
#include "seahorse-server-source.h"

#define UPDATING    "updating"

enum {
	PROP_0,
    PROP_GCONF_KEY,
    PROP_NONE_OPTION
};

/* Forward declaration */
static void populate_combo (SeahorseKeyserverControl *combo, gboolean gconf,
                            gboolean force);

static void    seahorse_keyserver_control_class_init      (SeahorseKeyserverControlClass *klass);
static void    seahorse_keyserver_control_init            (SeahorseKeyserverControl *skc);
static void    seahorse_keyserver_control_finalize        (GObject *gobject);
static void    seahorse_keyserver_control_set_property    (GObject *object, guint prop_id,
                                                           const GValue *value, GParamSpec *pspec);
static void    seahorse_keyserver_control_get_property    (GObject *object, guint prop_id,
                                                           GValue *value, GParamSpec *pspec);

static GtkComboBoxClass *parent_class = NULL;

GType
seahorse_keyserver_control_get_type (void)
{
    static GType control_type = 0;
    
    if (!control_type) {
        static const GTypeInfo control_info = {
            sizeof (SeahorseKeyserverControlClass), NULL, NULL,
            (GClassInitFunc) seahorse_keyserver_control_class_init,
            NULL, NULL, sizeof (SeahorseKeyserverControl), 0, (GInstanceInitFunc) seahorse_keyserver_control_init
        };
        
        control_type = g_type_register_static (GTK_TYPE_VBOX, "SeahorseKeyserverControl", &control_info, 0);
    }
    
    return control_type;
}

static void
seahorse_keyserver_control_class_init (SeahorseKeyserverControlClass *klass)
{
    GObjectClass *gobject_class;
    
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->set_property = seahorse_keyserver_control_set_property;
    gobject_class->get_property = seahorse_keyserver_control_get_property;
    gobject_class->finalize = seahorse_keyserver_control_finalize;
    
    g_object_class_install_property (gobject_class, PROP_NONE_OPTION,
            g_param_spec_string ("none-option", "No key option", "Puts in an option for 'no key server'",
                                  NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_GCONF_KEY,
            g_param_spec_string ("gconf-key", "GConf key", "GConf key to read/write selection",
                                  NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
}

static void
keyserver_changed (GtkComboBox *widget, SeahorseKeyserverControl *skc)
{
    /* If currently updating (see populate_combo) ignore */
    if (g_object_get_data (G_OBJECT (skc), UPDATING) != NULL)
        return;
    
    if (skc->gconf_key) {
        const gchar *t = seahorse_keyserver_control_selected (skc);
        seahorse_gconf_set_string (skc->gconf_key, t ? t : "");
    }
}

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
    SeahorseKeyserverControl *skc = SEAHORSE_KEYSERVER_CONTROL (data);   
    const gchar *key = gconf_entry_get_key (entry);

    if (g_str_equal (KEYSERVER_KEY, key))
        populate_combo (skc, FALSE, FALSE);
    else if (skc->gconf_key && g_str_equal (skc->gconf_key, key))
        populate_combo (skc, TRUE, FALSE);    
}

static void    
seahorse_keyserver_control_init (SeahorseKeyserverControl *skc)
{
    skc->combo = GTK_COMBO_BOX (gtk_combo_box_new_text ());
    gtk_container_add (GTK_CONTAINER (skc), GTK_WIDGET (skc->combo));
    gtk_widget_show (GTK_WIDGET (skc->combo));
 
    populate_combo (skc, TRUE, TRUE);
    g_signal_connect (skc->combo, "changed", G_CALLBACK (keyserver_changed), skc);
    skc->notify_id_list = seahorse_gconf_notify (KEYSERVER_KEY, gconf_notify, skc);
}

static void
seahorse_keyserver_control_set_property (GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec)
{
    SeahorseKeyserverControl *control = SEAHORSE_KEYSERVER_CONTROL (object);
    const gchar *t;
    
    switch (prop_id) {
    case PROP_GCONF_KEY:
        if (control->notify_id)
            seahorse_gconf_unnotify (control->notify_id);
        g_free (control->gconf_key);
        t = g_value_get_string (value);
        control->gconf_key = t ? g_strdup (t) : NULL;
        if (control->gconf_key) 
            control->notify_id = seahorse_gconf_notify (control->gconf_key, gconf_notify, control);
        populate_combo (control, TRUE, TRUE);
        break;
        
    case PROP_NONE_OPTION:
        g_free (control->none_option);
        t = g_value_get_string (value);
        control->none_option = t ? g_strdup (t) : NULL;
        populate_combo (control, TRUE, TRUE);
        break;
        
    default:
        break;
    }
}

static void
seahorse_keyserver_control_get_property (GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec)
{
    SeahorseKeyserverControl *control = SEAHORSE_KEYSERVER_CONTROL (object);
    
    switch (prop_id) {
        case PROP_NONE_OPTION:
            g_value_set_string (value, control->none_option);
            break;

        case PROP_GCONF_KEY:
            g_value_set_string (value, control->gconf_key);
            break;
        
        default:
            break;
    }
}


static void
seahorse_keyserver_control_finalize (GObject *gobject)
{
    SeahorseKeyserverControl *skc = SEAHORSE_KEYSERVER_CONTROL (gobject);
    
    if (skc->keyservers) {
        seahorse_util_string_slist_free (skc->keyservers);
        skc->keyservers = NULL;
    }
    
    if (skc->notify_id >= 0) {
        seahorse_gconf_unnotify (skc->notify_id);
        skc->notify_id = 0;
    }

    if (skc->notify_id_list >= 0) {
        seahorse_gconf_unnotify (skc->notify_id_list);
        skc->notify_id_list = 0;
    }

    g_free (skc->gconf_key);    
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
populate_combo (SeahorseKeyserverControl *skc, gboolean gconf, gboolean force)
{
    GSList *l, *ks;
    gint i, n;
    gchar *chosen = NULL;

    /* Get the appropriate selection */
    if (gconf && skc->gconf_key)
        chosen = seahorse_gconf_get_string (skc->gconf_key);
    else {
        n = gtk_combo_box_get_active (skc->combo);
        if (n > 0 && n <= g_slist_length (skc->keyservers))
            chosen = g_strdup ((gchar*)g_slist_nth_data (skc->keyservers, n - 1));
    }

    /* Mark this so we can ignore events */
    g_object_set_data (G_OBJECT (skc), UPDATING, GINT_TO_POINTER (1));
    
    /* Retreieve the key server list and make sure it's changed */
    ks = seahorse_gconf_get_string_list (KEYSERVER_KEY);
    ks = seahorse_server_source_purge_keyservers (ks);
    ks = g_slist_sort (ks, (GCompareFunc)g_utf8_collate);
    
    if (force || !seahorse_util_string_slist_equal (ks, skc->keyservers)) {
        
        /* Remove saved data */
        for (i = g_slist_length (skc->keyservers) + 1; i >= 0; i--)
            gtk_combo_box_remove_text (skc->combo, 0);

        /* Reload all the data */
        seahorse_util_string_slist_free (skc->keyservers);
        skc->keyservers = ks;
        ks = NULL;
        
        /* The all key servers option */
        if (skc->none_option)
            gtk_combo_box_prepend_text (skc->combo, skc->none_option);
    
        for (l = skc->keyservers; l != NULL; l = g_slist_next (l)) 
            gtk_combo_box_append_text (skc->combo, (gchar*)l->data);
    }

    if (chosen) {

        n = (skc->none_option ? 0 : -1);
        for (i = 0, l = skc->keyservers; l != NULL; l = g_slist_next (l), i++) {
            if (g_utf8_collate ((gchar*)l->data, chosen) == 0)
                n = i + (skc->none_option ? 1 : 0);
        }
        
        if (gtk_combo_box_get_active (skc->combo) != n)
            gtk_combo_box_set_active (skc->combo, n);
        
    } else if (skc->none_option) {
        gtk_combo_box_set_active (skc->combo, 0);
    }
    
    g_free (chosen);
    seahorse_util_string_slist_free (ks);
    
    /* Done updating */
    g_object_set_data (G_OBJECT (skc), UPDATING, NULL);    
}

SeahorseKeyserverControl*  
seahorse_keyserver_control_new (const gchar *gconf_key, const gchar *none_option)
{
    return g_object_new (SEAHORSE_TYPE_KEYSERVER_CONTROL, 
                         "gconf-key", gconf_key, "none-option", none_option, NULL);
}

const gchar *          
seahorse_keyserver_control_selected (SeahorseKeyserverControl *skc)
{
    gint n;
    
    n = gtk_combo_box_get_active (skc->combo);
    g_return_val_if_fail (n >= 0, NULL);
    
    if (skc->none_option)
        return n > 0 ? g_slist_nth_data (skc->keyservers, n - 1) : NULL;
    else 
        return g_slist_nth_data (skc->keyservers, n);
}
