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

/* Special fields */
enum {
    FIELD_DISLAY_NAME,
    FIELD_CN,
    FIELD_SPECIAL_MAX
};

GQuark key_fields[FIELD_SPECIAL_MAX] = { 0 };

enum {
    KEY_ADDED,
    KEY_REMOVED,
    KEY_CHANGED,
    LAST_SIGNAL
};

G_DEFINE_TYPE (SeahorseServiceKeyset, seahorse_service_keyset, SEAHORSE_TYPE_KEYSET);
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
lookup_key_field (SeahorseKey *skey, guint uid, const gchar *field, GValue *value)
{
    GParamSpec *spec;
    GQuark qfield;
    gchar *name;
    
    /* Special UID fields */
    if (uid > 0) {

        qfield = g_quark_from_string (field);
        if (qfield == key_fields[FIELD_DISLAY_NAME]) {
            name = seahorse_key_get_name (skey, uid);
            g_value_init (value, G_TYPE_STRING);
            g_value_take_string (value, name ? name : g_strdup (""));
            return TRUE;

        } else if (qfield == key_fields[FIELD_CN]) {
            name = seahorse_key_get_name_cn (skey, uid);
            g_value_init (value, G_TYPE_STRING);
            g_value_take_string (value, name ? name : g_strdup (""));
            return TRUE;
        }
    }

    spec = g_object_class_find_property (G_OBJECT_GET_CLASS (skey), field);
    if (!spec) 
        return FALSE;

    g_value_init (value, spec->value_type);
    g_object_get_property (G_OBJECT (skey), field, value);
    return TRUE; 
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
    
    k = seahorse_keyset_get_keys (SEAHORSE_KEYSET (keyset));
    a = g_array_new (TRUE, TRUE, sizeof (gchar*));
    
    for (l = k; l; l = g_list_next (l)) {
        id = seahorse_service_key_to_dbus (SEAHORSE_KEY (l->data), 0);
        g_array_append_val (a, id);
        
        nuids = seahorse_key_get_num_names (SEAHORSE_KEY (l->data));
        for (i = 1; i < nuids; i++) {
            id = seahorse_service_key_to_dbus (SEAHORSE_KEY (l->data), i);
            g_array_append_val (a, id);
        }
    }
    
    *keys = (gchar**)g_array_free (a, FALSE);
    return TRUE;
}

gboolean
seahorse_service_keyset_get_key_field (SeahorseService *svc, gchar *key, gchar *field,
                                       gboolean *has, GValue *value, GError **error)
{
    SeahorseKey *skey;
    guint uid;

    skey = seahorse_service_key_from_dbus (key, &uid);
    if (!skey) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                     _("Invalid or unrecognized key: %s"), key);
        return FALSE;
    }
 
    if (lookup_key_field (skey, uid, field, value)) {
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
    SeahorseKey *skey;
    GValue *value;
    guint uid;
    
    skey = seahorse_service_key_from_dbus (key, &uid);
    if (!skey) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                     _("Invalid or unrecognized key: %s"), key);
        return FALSE;
    }

    *values = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, value_free);
    
    while (*fields) {
        value = g_new0 (GValue, 1);
        if (lookup_key_field (skey, uid, *fields, value))
            g_hash_table_insert (*values, g_strdup (*fields), value);
        else
            g_free (value);
        fields++;
    }
    
    return TRUE; 
}

gboolean        
seahorse_service_keyset_discover_keys (SeahorseServiceKeyset *keyset, const gchar **keyids, 
                                       gint flags, gchar ***keys, GError **error)
{
    GArray *akeys = NULL;
    gchar *keyid = NULL;
    GSList *todiscover = NULL;
    GList *toimport = NULL;
    SeahorseKey* skey;
    SeahorseKeyLoc loc;
    SeahorseOperation *op;
    const gchar **k;
    gchar *t;
    GSList *l;
    gboolean ret = FALSE;
    
    akeys = g_array_new (TRUE, TRUE, sizeof (gchar*));
    
    /* Check all the keyids */
    for (k = keyids; *k; k++) {
        
        g_free (keyid);
        
        keyid = seahorse_key_source_cannonical_keyid (keyset->ktype, *k);
        if (!keyid) {
            g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                         _("Invalid key id: %s"), *k);
            goto finally;
        }
        
        /* Do we know about this key? */
        skey = seahorse_context_find_key (SCTX_APP (), keyset->ktype, 
                                          SKEY_LOC_INVALID, keyid);

        /* Add to the return value */
        t = seahorse_service_keyid_to_dbus (keyset->ktype, keyid, 0);
        g_array_append_val (akeys, t);
        
        /* No such key anywhere, discover it */
        if (!skey) {
            todiscover = g_slist_prepend (todiscover, keyid);
            keyid = NULL;
            continue;
        }
        
        g_free (keyid);
        keyid = NULL;
        
        /* We know about this key, check where it is */
        loc = seahorse_key_get_location (skey);
        g_assert (loc != SKEY_LOC_INVALID);
        
        /* Do nothing for local keys */
        if (loc >= SKEY_LOC_LOCAL)
            continue;
        
        /* Remote keys get imported */
        else if (loc >= SKEY_LOC_REMOTE)
            toimport = g_list_prepend (toimport, skey);
        
        /* Searching keys are ignored */
        else if (loc >= SKEY_LOC_SEARCHING)
            continue;
        
        /* Not found keys are tried again */
        else if (loc >= SKEY_LOC_UNKNOWN) {
            todiscover = g_slist_prepend (todiscover, keyid);
            keyid = NULL;
        }
    }
    
    /* Start an import process on all toimport */
    if (toimport) {
        op = seahorse_context_transfer_keys (SCTX_APP (), toimport, NULL);
        /* Running operations ref themselves */
        g_object_unref (op);
    }
    
    /* Start a discover process on all todiscover */
    if (todiscover) {
        op = seahorse_context_retrieve_keys (SCTX_APP (), keyset->ktype, 
                                             todiscover, NULL);
        /* Running operations ref themselves */
        g_object_unref (op);
    }

    ret = TRUE;
    
finally:
    if (todiscover) {
        for (l = todiscover; l; l = g_slist_next (l)) 
            g_free (l->data);
        g_slist_free (todiscover);
    }
    
    if (toimport)
        g_list_free (toimport);
        
    if (keyid)
        g_free (keyid);
    
    if (ret) {
        g_assert (akeys);
        *keys = (gchar**)g_array_free (akeys, FALSE);
    } else if (akeys) {
        g_strfreev ((gchar**)g_array_free (akeys, FALSE));
    }
    
    return ret;
}

/* -----------------------------------------------------------------------------
 * DBUS SIGNALS 
 */

static void
seahorse_service_keyset_added (SeahorseKeyset *skset, SeahorseKey *skey, 
                               gpointer userdata)
{
    gchar *id;
    guint uids, i;
    
    uids = seahorse_key_get_num_names (skey);
    uids = (uids == 0) ? 1 : uids;

    for (i = 0; i < uids; i++) {    
        id = seahorse_service_key_to_dbus (skey, i);
        g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_ADDED], 0, id);
        g_free (id);        
    }
    
    seahorse_keyset_set_closure (skset, skey, GUINT_TO_POINTER (uids));
}

static void
seahorse_service_keyset_removed (SeahorseKeyset *skset, SeahorseKey *skey, 
                                 gpointer closure, gpointer userdata)
{
    gchar *id;
    guint uids, i;

    uids = GPOINTER_TO_UINT (closure);
    uids = (uids == 0) ? 1 : uids;
    
    for (i = 0; i < uids; i++) {
        id = seahorse_service_key_to_dbus (skey, i);
        g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_REMOVED], 0, id);
        g_free (id);
    }
}

static void
seahorse_service_keyset_changed (SeahorseKeyset *skset, SeahorseKey *skey, 
                                 SeahorseKeyChange change, gpointer closure, 
                                 gpointer userdata)
{
    gchar *id;
    guint uids, euids, i;
    
    /* Adding or removing uids means we do a add/remove */
    uids = seahorse_key_get_num_names (skey);
    uids = (uids == 0) ? 1 : uids;
    
    euids = GPOINTER_TO_UINT (closure);
    if (euids != uids)
        seahorse_keyset_set_closure (skset, skey, GUINT_TO_POINTER (uids));

g_printerr ("old: %d / current: %d\n", euids, uids);
    if (euids < uids) {
        for (i = euids; i < uids; i++) {
            id = seahorse_service_key_to_dbus (skey, i);
            g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_ADDED], 0, id);
            g_free (id);
        }
    } else if (euids > uids) {
        for (i = uids; i < euids; i++) {
            id = seahorse_service_key_to_dbus (skey, i);
            g_signal_emit (SEAHORSE_SERVICE_KEYSET (skset), signals[KEY_REMOVED], 0, id);
            g_free (id);
        }
    }

    for (i = 0; i < uids; i++) {
        id = seahorse_service_key_to_dbus (skey, i);
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
    
    
    /* Some special fields */
    key_fields[FIELD_DISLAY_NAME] = g_quark_from_static_string ("display-name");
    key_fields[FIELD_CN] = g_quark_from_static_string ("cn");
}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS
 */
 
SeahorseKeyset* 
seahorse_service_keyset_new (GQuark ktype, SeahorseKeyLoc location)
{
    SeahorseServiceKeyset *skset;
    SeahorseKeyPredicate *pred = g_new0(SeahorseKeyPredicate, 1);
    
    pred->ktype = ktype;
    pred->location = location;
    
    skset = g_object_new (SEAHORSE_TYPE_SERVICE_KEYSET, "predicate", pred, NULL);
    g_object_set_data_full (G_OBJECT (skset), "quick-predicate", pred, g_free);
    
    skset->ktype = ktype;
    return SEAHORSE_KEYSET (skset);
}
