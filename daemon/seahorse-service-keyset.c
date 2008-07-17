/*
 * Seahorse
 *
 * Copyright (C) 2005, 2006 Stefan Walter
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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "seahorse-key.h"
#include "seahorse-service.h"
#include "seahorse-util.h"

/* Flags for seahorse_service_keyset_match_keys */
#define MATCH_KEYS_LOCAL_ONLY       0x00000010

enum {
    KEY_ADDED,
    KEY_REMOVED,
    KEY_CHANGED,
    LAST_SIGNAL
};

G_DEFINE_TYPE (SeahorseServiceKeyset, seahorse_service_keyset, SEAHORSE_TYPE_SET);
static guint signals[LAST_SIGNAL] = { 0 };

/* -----------------------------------------------------------------------------
 * HELPERS 
 */

static void
value_free (gpointer value)
{
    g_value_unset ((GValue*)value);
    g_free (value);
}

static gboolean
match_remove_patterns (GArray *patterns, gchar *value)
{
    gboolean matched = FALSE;
    gchar *lower;
    int i;
    
    if(value) {
        lower = g_utf8_casefold (value, -1);
        g_free (value);
        
        /* 
         * Search for each pattern on this key. We try to match
         * as many as possible.
         */
        for (i = 0; i < patterns->len; i++) {
            if (strstr (lower, g_array_index (patterns, gchar*, i)) ? TRUE : FALSE) {
                matched = TRUE;
                g_free (g_array_index (patterns, gchar*, i));
                g_array_remove_index_fast (patterns, i);
                i--;
            }
        }
        
        g_free (lower);
    }
    
    return matched;
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
	guint nuids, i;
    
	k = seahorse_set_get_objects (SEAHORSE_SET (keyset));
	a = g_array_new (TRUE, TRUE, sizeof (gchar*));
    
	for (l = k; l; l = g_list_next (l)) {
		id = seahorse_context_object_to_dbus (SCTX_APP (), SEAHORSE_OBJECT (l->data), 0);
		g_array_append_val (a, id);
        
		if (SEAHORSE_IS_KEY (l->data)) {
			nuids = seahorse_key_get_num_names (SEAHORSE_KEY (l->data));
			for (i = 1; i < nuids; i++) {
				id = seahorse_context_object_to_dbus (SCTX_APP (), SEAHORSE_OBJECT (l->data), i);
				g_array_append_val (a, id);
			}
		}
	}
    
    *keys = (gchar**)g_array_free (a, FALSE);
    return TRUE;
}

gboolean
seahorse_service_keyset_get_key_field (SeahorseService *svc, gchar *key, gchar *field,
                                       gboolean *has, GValue *value, GError **error)
{
    SeahorseObject *sobj;
    guint uid;

    sobj = seahorse_context_object_from_dbus (SCTX_APP(), key, &uid);
    if (!sobj) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                     _("Invalid or unrecognized key: %s"), key);
        return FALSE;
    }
 
    if (SEAHORSE_IS_KEY (sobj) && 
        seahorse_key_lookup_property (SEAHORSE_KEY (sobj), uid, field, value)) {
        *has = TRUE;
        
    } else {
        *has = FALSE;
        
        /* As close as we can get to 'null' */
        g_value_init (value, G_TYPE_INT);
        g_value_set_int (value, 0);
    }

    return TRUE;
}

gboolean
seahorse_service_keyset_get_key_fields (SeahorseService *svc, gchar *key, gchar **fields,
                                        GHashTable **values, GError **error)
{
	SeahorseObject *sobj;
	GValue *value;
	guint uid;
    
	sobj = seahorse_context_object_from_dbus (SCTX_APP(), key, &uid);
	if (!sobj) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
		             _("Invalid or unrecognized key: %s"), key);
		return FALSE;
	}

	*values = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, value_free);
    
	if (SEAHORSE_IS_KEY (sobj)) {
		while (*fields) {
			value = g_new0 (GValue, 1);
			if (seahorse_key_lookup_property (SEAHORSE_KEY (sobj), uid, *fields, value))
				g_hash_table_insert (*values, g_strdup (*fields), value);
			else
				g_free (value);
			fields++;
		}
	}
	
	return TRUE; 
}

gboolean        
seahorse_service_keyset_discover_keys (SeahorseServiceKeyset *keyset, const gchar **keyids, 
                                       gint flags, gchar ***keys, GError **error)
{
    GArray *akeys = NULL;
    GList *lkeys, *l;
    GSList *rawids = NULL;
    const gchar **k;
    GQuark keyid;
    gchar *id;
    
    /* Check to make sure the key ids are valid */
    for (k = keyids; *k; k++) {
        keyid = seahorse_source_canonize_id (keyset->ktype, *k);
        if (!keyid) {
            g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                         _("Invalid key id: %s"), *k);
            return FALSE;
        }
        rawids = g_slist_prepend (rawids, (char*)(*k));
    }
    
    /* Pass it on to do the work */
    lkeys = seahorse_context_discover_objects (SCTX_APP (), keyset->ktype, rawids);
    g_slist_free (rawids);
    
    /* Prepare return value */
    akeys = g_array_new (TRUE, TRUE, sizeof (gchar*));
    for (l = lkeys; l; l = g_list_next (l)) {
        id = seahorse_context_object_to_dbus (SCTX_APP(), SEAHORSE_OBJECT (l->data), 0);
        akeys = g_array_append_val (akeys, id);
    }
    *keys = (gchar**)g_array_free (akeys, FALSE);
    g_list_free (lkeys);
    
    return TRUE;
}

gboolean
seahorse_service_keyset_match_keys (SeahorseServiceKeyset *keyset, gchar **patterns, 
                                    gint flags, gchar ***keys, gchar***unmatched, 
                                    GError **error)
{
    GArray *results = NULL;
    GArray *remaining = NULL;
    GList *lkeys, *l;
    SeahorseKey *skey;
    gchar *value, *id;
    gchar **k;
    int i, nnames;

    /* Copy the keys so we can see what's left. */
    remaining = g_array_new (TRUE, TRUE, sizeof (gchar*));
    for(k = patterns; *k; k++) {
        value = g_utf8_casefold (*k, -1);
        g_array_append_val (remaining, value);
    }
    
    results = g_array_new (TRUE, TRUE, sizeof (gchar*));
    

    lkeys = seahorse_context_find_objects (SCTX_APP (), keyset->ktype, SEAHORSE_USAGE_NONE, 
                                           (flags & MATCH_KEYS_LOCAL_ONLY) ? SEAHORSE_LOCATION_LOCAL : SEAHORSE_LOCATION_INVALID);
    for (l = lkeys; l; l = g_list_next(l)) {
        
        if (remaining->len <= 0)
            break;

        if (!SEAHORSE_IS_KEY (l->data))
            continue;
        skey = SEAHORSE_KEY (l->data);
        
        /* Note that match_remove_patterns frees value */
        
        /* Main name */        
        value = seahorse_key_get_display_name (skey);
        if (match_remove_patterns (remaining, value)) {
            id = seahorse_context_object_to_dbus (SCTX_APP (), SEAHORSE_OBJECT (skey), 0);
            g_array_append_val (results, id);
        }
        
        /* Key ID */
        value = g_strdup (seahorse_key_get_rawid (seahorse_key_get_keyid (skey)));
        if (match_remove_patterns (remaining, value)) {
            id = seahorse_context_object_to_dbus (SCTX_APP (), SEAHORSE_OBJECT (skey), 0);
            g_array_append_val (results, id);
        }
        
        /* Each UID */
        nnames = seahorse_key_get_num_names (skey);
        for (i = 0; i < nnames; i++) {
            
            /* UID name */
            value = seahorse_key_get_name (skey, i);
            if (match_remove_patterns (remaining, value)) {
                id = seahorse_context_object_to_dbus (SCTX_APP (), SEAHORSE_OBJECT (skey), i);
                g_array_append_val (results, id);
                break;
            }
            
            /* UID email */
            value = seahorse_key_get_name_cn (skey, i);
            if (match_remove_patterns (remaining, value)) {
                id = seahorse_context_object_to_dbus (SCTX_APP (), SEAHORSE_OBJECT (skey), i);
                g_array_append_val (results, id);
                break;
            }
        }
    }
    
    /* Sort results and remove duplicates */
    g_array_sort (results, (GCompareFunc)g_utf8_collate);
    for (i = 0; i < results->len; i++) {
        if (i > 0 && i < results->len) {
            if (strcmp (g_array_index (results, gchar*, i), 
                        g_array_index (results, gchar*, i - 1)) == 0) {
                g_free (g_array_index (results, gchar*, i));
                g_array_remove_index (results, i);
                i--;
            }
        }
    }
    
    *keys = (gchar**)g_array_free (results, FALSE);
    *unmatched = (gchar**)g_array_free (remaining, FALSE);

    return TRUE;
}

/* -----------------------------------------------------------------------------
 * DBUS SIGNALS 
 */

static void
seahorse_service_keyset_added (SeahorseSet *skset, SeahorseObject *sobj, 
                               gpointer userdata)
{
	gchar *id;
	guint uids = 0, i;
    
	if (SEAHORSE_IS_KEY (sobj))
		uids = seahorse_key_get_num_names (SEAHORSE_KEY (sobj));
	uids = (uids == 0) ? 1 : uids;

	for (i = 0; i < uids; i++) {    
		id = seahorse_context_object_to_dbus (SCTX_APP (), sobj, i);
		g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_ADDED], 0, id);
		g_free (id);        
	}
    
	seahorse_set_set_closure (skset, sobj, GUINT_TO_POINTER (uids));
}

static void
seahorse_service_keyset_removed (SeahorseSet *skset, SeahorseObject *sobj, 
                                 gpointer closure, gpointer userdata)
{
    gchar *id;
    guint uids, i;

    uids = GPOINTER_TO_UINT (closure);
    uids = (uids == 0) ? 1 : uids;
    
    for (i = 0; i < uids; i++) {
        id = seahorse_context_object_to_dbus (SCTX_APP (), sobj, i);
        g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_REMOVED], 0, id);
        g_free (id);
    }
}

static void
seahorse_service_keyset_changed (SeahorseSet *skset, SeahorseObject *sobj, 
                                 SeahorseObjectChange change, gpointer closure, 
                                 gpointer userdata)
{
    gchar *id;
    guint uids = 0, euids, i;
    
    /* Adding or removing uids means we do a add/remove */
    if (SEAHORSE_IS_KEY (sobj))
        uids = seahorse_key_get_num_names (SEAHORSE_KEY (sobj));
    uids = (uids == 0) ? 1 : uids;
    
    euids = GPOINTER_TO_UINT (closure);
    if (euids != uids)
        seahorse_set_set_closure (skset, sobj, GUINT_TO_POINTER (uids));

    if (euids < uids) {
        for (i = euids; i < uids; i++) {
            id = seahorse_context_object_to_dbus (SCTX_APP (), sobj, i);
            g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_ADDED], 0, id);
            g_free (id);
        }
    } else if (euids > uids) {
        for (i = uids; i < euids; i++) {
            id = seahorse_context_object_to_dbus (SCTX_APP (), sobj, i);
            g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_REMOVED], 0, id);
            g_free (id);
        }
    }

    for (i = 0; i < uids; i++) {
        id = seahorse_context_object_to_dbus (SCTX_APP (), sobj, i);
        g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_CHANGED], 0, id);
        g_free (id);
    }
    
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_service_keyset_init (SeahorseServiceKeyset *keyset)
{
    g_signal_connect_after (keyset, "added", G_CALLBACK (seahorse_service_keyset_added), NULL);
    g_signal_connect_after (keyset, "removed", G_CALLBACK (seahorse_service_keyset_removed), NULL);
    g_signal_connect_after (keyset, "changed", G_CALLBACK (seahorse_service_keyset_changed), NULL);
}

static void
seahorse_service_keyset_class_init (SeahorseServiceKeysetClass *klass)
{
    GObjectClass *gclass;

    gclass = G_OBJECT_CLASS (klass);
    
    signals[KEY_ADDED] = g_signal_new ("key_added", SEAHORSE_TYPE_SERVICE_KEYSET, 
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, G_STRUCT_OFFSET (SeahorseServiceKeysetClass, key_added),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
    
    signals[KEY_REMOVED] = g_signal_new ("key_removed", SEAHORSE_TYPE_SERVICE_KEYSET, 
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, G_STRUCT_OFFSET (SeahorseServiceKeysetClass, key_removed),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
    
    signals[KEY_CHANGED] = g_signal_new ("key_changed", SEAHORSE_TYPE_SERVICE_KEYSET, 
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, G_STRUCT_OFFSET (SeahorseServiceKeysetClass, key_changed),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */
 
SeahorseSet* 
seahorse_service_keyset_new (GQuark ktype, SeahorseLocation location)
{
    SeahorseServiceKeyset *skset;
    SeahorseObjectPredicate *pred = g_new0(SeahorseObjectPredicate, 1);
    
    pred->tag = ktype;
    pred->location = location;
    
    skset = g_object_new (SEAHORSE_TYPE_SERVICE_KEYSET, "predicate", pred, NULL);
    g_object_set_data_full (G_OBJECT (skset), "quick-predicate", pred, g_free);
    
    skset->ktype = ktype;
    return SEAHORSE_SET (skset);
}
