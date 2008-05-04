/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include "pgp/seahorse-server-source.h"

#define UPDATING    "updating"

enum {
    PROP_0,
    PROP_GCONF_KEY,
    PROP_NONE_OPTION
};

enum {
    COL_TEXT,
    COL_INFO
};

enum {
    OPTION_NONE,
    OPTION_SEPARATOR,
    OPTION_KEYSERVER
};

/* Forward declaration */
static void populate_combo (SeahorseKeyserverControl *combo, gboolean gconf);

static void    seahorse_keyserver_control_class_init      (SeahorseKeyserverControlClass *klass);
static void    seahorse_keyserver_control_init            (SeahorseKeyserverControl *skc);
static GObject *seahorse_keyserver_control_constructor    (GType type, guint n_construct_properties,
                                                           GObjectConstructParam *construct_params);
static void    seahorse_keyserver_control_finalize        (GObject *gobject);
static void    seahorse_keyserver_control_set_property    (GObject *object, guint prop_id,
                                                           const GValue *value, GParamSpec *pspec);
static void    seahorse_keyserver_control_get_property    (GObject *object, guint prop_id,
                                                           GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE(SeahorseKeyserverControl, seahorse_keyserver_control, GTK_TYPE_COMBO_BOX)

static void
seahorse_keyserver_control_class_init (SeahorseKeyserverControlClass *klass)
{
    GObjectClass *gobject_class;
    
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->constructor = seahorse_keyserver_control_constructor;
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
        gchar *t = seahorse_keyserver_control_selected (skc);
        seahorse_gconf_set_string (skc->gconf_key, t ? t : "");
        g_free (t);
    }
}

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
    SeahorseKeyserverControl *skc = SEAHORSE_KEYSERVER_CONTROL (data);   
    const gchar *key = gconf_entry_get_key (entry);

    if (g_str_equal (KEYSERVER_KEY, key))
        populate_combo (skc, FALSE);
    else if (skc->gconf_key && g_str_equal (skc->gconf_key, key))
        populate_combo (skc, TRUE);
}

static gint
compare_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
    gchar *desc_a, *desc_b;
    gint info_a, info_b;
    gint retval;

    gtk_tree_model_get (model, a, COL_TEXT, &desc_a, COL_INFO, &info_a, -1);
    gtk_tree_model_get (model, b, COL_TEXT, &desc_b, COL_INFO, &info_b, -1);

    if (info_a != info_b)
        retval = info_a - info_b;
    else if (info_a == OPTION_KEYSERVER)
        retval = g_utf8_collate (desc_a, desc_b);
    else
        retval = 0;

    g_free (desc_a);
    g_free (desc_b);

    return retval;
}

static gboolean
separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    gint info;

    gtk_tree_model_get (model, iter, COL_INFO, &info, -1);
     
    return info == OPTION_SEPARATOR;
}

static void    
seahorse_keyserver_control_init (SeahorseKeyserverControl *skc)
{
    GtkCellRenderer *renderer;

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (skc), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (skc), renderer,
                                    "text", COL_TEXT,
                                    NULL);
    gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (skc),
                                          (GtkTreeViewRowSeparatorFunc) separator_func,
                                          NULL, NULL);
}

static GObject *
seahorse_keyserver_control_constructor (GType type, guint n_construct_properties,
                                        GObjectConstructParam *construct_params)
{
    GObject *object;
    SeahorseKeyserverControl *skc;

    object = G_OBJECT_CLASS (seahorse_keyserver_control_parent_class)->constructor
                                (type, n_construct_properties, construct_params);
    skc = SEAHORSE_KEYSERVER_CONTROL (object);

    populate_combo (skc, TRUE);
    g_signal_connect (skc, "changed", G_CALLBACK (keyserver_changed), skc);
    skc->notify_id_list = seahorse_gconf_notify (KEYSERVER_KEY, gconf_notify, skc);
    if (skc->gconf_key)
        skc->notify_id = seahorse_gconf_notify (skc->gconf_key, gconf_notify, skc);

    return object;
}

static void
seahorse_keyserver_control_set_property (GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec)
{
    SeahorseKeyserverControl *control = SEAHORSE_KEYSERVER_CONTROL (object);
    
    switch (prop_id) {
    case PROP_GCONF_KEY:
        control->gconf_key = g_value_dup_string (value);
        break;
        
    case PROP_NONE_OPTION:
        control->none_option = g_value_dup_string (value);
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
    
    if (skc->notify_id >= 0) {
        seahorse_gconf_unnotify (skc->notify_id);
        skc->notify_id = 0;
    }

    if (skc->notify_id_list >= 0) {
        seahorse_gconf_unnotify (skc->notify_id_list);
        skc->notify_id_list = 0;
    }

    g_free (skc->gconf_key);

    G_OBJECT_CLASS (seahorse_keyserver_control_parent_class)->finalize (gobject);
}

static void
populate_combo (SeahorseKeyserverControl *skc, gboolean gconf)
{
    GtkComboBox *combo = GTK_COMBO_BOX (skc);
    GSList *l, *ks;
    gchar *chosen = NULL;
    gint chosen_info = OPTION_KEYSERVER;
    GtkListStore *store;
    GtkTreeIter iter, none_iter, chosen_iter;
    gboolean chosen_iter_set = FALSE;

    /* Get the appropriate selection */
    if (gconf && skc->gconf_key)
        chosen = seahorse_gconf_get_string (skc->gconf_key);
    else {
        if (gtk_combo_box_get_active_iter (combo, &iter)) {
            gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
                                COL_TEXT, &chosen,
                                COL_INFO, &chosen_info,
                                -1);
        }
    }

    /* Mark this so we can ignore events */
    g_object_set_data (G_OBJECT (skc), UPDATING, GINT_TO_POINTER (1));
    
    /* Remove old model, and create new one */
    gtk_combo_box_set_model (combo, NULL);

    store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);

    /* The all key servers option */
    if (skc->none_option) {
        gtk_list_store_insert_with_values (store, &none_iter, 0,
                                           COL_TEXT, skc->none_option,
                                           COL_INFO, OPTION_NONE,
                                           -1);
        /* And add a separator row */
        gtk_list_store_insert_with_values (store, &iter, 0,
                                           COL_TEXT, NULL,
                                           COL_INFO, OPTION_SEPARATOR,
                                           -1);
    }

    ks = seahorse_gconf_get_string_list (KEYSERVER_KEY);
    ks = seahorse_server_source_purge_keyservers (ks);

    for (l = ks; l != NULL; l = g_slist_next (l)) {
        const gchar *keyserver = (const gchar *) l->data;

        g_assert (keyserver != NULL);
        gtk_list_store_insert_with_values (store, &iter, 0,
                                           COL_TEXT, keyserver,
                                           COL_INFO, OPTION_KEYSERVER,
                                           -1);
        if (chosen && g_strcmp0 (chosen, keyserver) == 0) {
            chosen_iter = iter;
            chosen_iter_set = TRUE;
        }
    }
    seahorse_util_string_slist_free (ks);
    g_free (chosen);

    /* Turn on sorting after populating the store, since that's faster */
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), COL_TEXT,
                                     (GtkTreeIterCompareFunc) compare_func,
                                     NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), COL_TEXT,
                                          GTK_SORT_ASCENDING);

    gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));

    /* Re-set the selected value, if it's still present in the model */
    if (chosen_iter_set)
        gtk_combo_box_set_active_iter (combo, &chosen_iter);
    else if (skc->none_option)
        gtk_combo_box_set_active_iter (combo, &none_iter);

    /* Done updating */
    g_object_set_data (G_OBJECT (skc), UPDATING, NULL);    
}

SeahorseKeyserverControl*  
seahorse_keyserver_control_new (const gchar *gconf_key, const gchar *none_option)
{
    return g_object_new (SEAHORSE_TYPE_KEYSERVER_CONTROL, 
                         "gconf-key", gconf_key, "none-option", none_option, NULL);
}

gchar *
seahorse_keyserver_control_selected (SeahorseKeyserverControl *skc)
{
    GtkComboBox *combo = GTK_COMBO_BOX (skc);
    GtkTreeIter iter;
    gint info;
    gchar *server;

    if (!gtk_combo_box_get_active_iter (combo, &iter))
        return NULL;

    gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
                        COL_TEXT, &server,
                        COL_INFO, &info,
                        -1);

    if (info == OPTION_KEYSERVER)
        return server;
        
    return NULL;
}
