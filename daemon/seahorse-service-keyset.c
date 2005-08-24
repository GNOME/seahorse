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

#include <glib.h>

#include "seahorse-service.h"

enum {
    KEY_ADDED,
    KEY_REMOVED,
    KEY_CHANGED,
    LAST_SIGNAL
};

G_DEFINE_TYPE (SeahorseServiceKeyset, seahorse_service_keyset, SEAHORSE_TYPE_KEYSET);
static SeahorseKeysetClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */
 
SeahorseKeyset* 
seahorse_service_keyset_new (GQuark ktype)
{
    SeahorseServiceKeyset *skset;
    SeahorseKeyPredicate *pred = g_new0(SeahorseKeyPredicate, 1);
    
    pred->ktype = ktype;
    
    skset = g_object_new (SEAHORSE_TYPE_SERVICE_KEYSET, "predicate", pred, NULL);
    g_object_set_data_full (G_OBJECT (skset), "quick-predicate", pred, g_free);
    
    return SEAHORSE_KEYSET (skset);
}

/* -----------------------------------------------------------------------------
 * DBUS METHODS 
 */

gboolean        
seahorse_service_keyset_list_keys (SeahorseServiceKeyset *keyset, gchar ***keys, 
                                   GError **error)
{
    GList *k, *l;
    GArray *a;
    gchar *id;
    
    k = seahorse_keyset_get_keys (SEAHORSE_KEYSET (keyset));
    a = g_array_new (TRUE, TRUE, sizeof (gchar*));
    
    for (l = k; l; l = g_list_next (l)) {
        id = seahorse_service_key_to_dbus (SEAHORSE_KEY (l->data));
        g_array_append_val (a, id);
    }
    
    *keys = (gchar**)g_array_free (a, FALSE);
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * DBUS SIGNALS 
 */

static void
seahorse_service_keyset_added (SeahorseKeyset *skset, SeahorseKey *skey)
{
    gchar *id = seahorse_service_key_to_dbus (skey);
    g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_ADDED], 0, id);
    g_free (id);        
}

static void
seahorse_service_keyset_removed (SeahorseKeyset *skset, SeahorseKey *skey, 
                              gpointer closure)
{
    gchar *id = seahorse_service_key_to_dbus (skey);
    g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_REMOVED], 0, id);
    g_free (id);
}

static void
seahorse_service_keyset_changed (SeahorseKeyset *skset, SeahorseKey *skey, 
                              SeahorseKeyChange change, gpointer closure)
{
    gchar *id = seahorse_service_key_to_dbus (skey);
    g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_CHANGED], 0, id);
    g_free (id);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_service_keyset_class_init (SeahorseServiceKeysetClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    parent_class->added = seahorse_service_keyset_added;
    parent_class->removed = seahorse_service_keyset_removed;
    parent_class->changed = seahorse_service_keyset_changed;
    
    gobject_class = G_OBJECT_CLASS (klass);
    
    signals[KEY_ADDED] = g_signal_new ("key_added", SEAHORSE_TYPE_SERVICE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseServiceKeysetClass, key_added),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
    
    signals[KEY_REMOVED] = g_signal_new ("key_removed", SEAHORSE_TYPE_SERVICE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseServiceKeysetClass, key_removed),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
    
    signals[KEY_CHANGED] = g_signal_new ("key_changed", SEAHORSE_TYPE_SERVICE_KEYSET, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseServiceKeysetClass, key_changed),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
seahorse_service_keyset_init (SeahorseServiceKeyset *keyset)
{

}
