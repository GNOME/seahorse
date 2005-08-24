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
#include <dbus/dbus-glib-bindings.h>

#include "seahorse-daemon.h"
#include "seahorse-service.h"
#include "seahorse-context.h"
#include "seahorse-key-source.h"
#include "seahorse-gpgmex.h"
#include "seahorse-util.h"

#define KEYSET_PATH "/org/gnome/seahorse/keys/%s"

G_DEFINE_TYPE (SeahorseService, seahorse_service, G_TYPE_OBJECT);
GObjectClass *parent_class = NULL;

static void
copy_to_array (const gchar *type, gpointer dummy, GArray *a)
{
    gchar *v = g_strdup (type);
    g_array_append_val (a, v);
}

static void
value_free (gpointer value)
{
    g_value_unset ((GValue*)value);
    g_free (value);
}

/* -----------------------------------------------------------------------------
 * PUBLIC METHODS 
 */ 

SeahorseKey*
seahorse_service_key_from_dbus (const gchar *key)
{
    gchar *ktype = NULL;
    const gchar *k;
    
    k = strchr(key, ':');
    if (!k)
        return NULL;
    
    /* The two parts */
    ktype = g_strndup (key, k - key);
    key = k + 1;
    
    return seahorse_context_find_key (SCTX_APP (), g_quark_from_string (ktype), 
                                      SKEY_LOC_UNKNOWN, key);
}

gchar*
seahorse_service_key_to_dbus (SeahorseKey *skey)
{
    return g_strdup_printf ("%s:%s", g_quark_to_string (seahorse_key_get_ktype (skey)),
                            seahorse_key_get_keyid (skey));    
}

/* -----------------------------------------------------------------------------
 * DBUS METHODS 
 */

gboolean 
seahorse_service_get_key_types (SeahorseService *svc, gchar ***ret, 
                                GError **error)
{
    GArray *a;
    
    if (svc->keysets) {
        a = g_array_new (TRUE, TRUE, sizeof (gchar*));
        g_hash_table_foreach (svc->keysets, (GHFunc)copy_to_array, a);
        *ret = (gchar**)g_array_free (a, FALSE);
        
    /* No keysets */
    } else {
        *ret = (gchar**)g_new0 (gchar*, 1);
    }
    
    return TRUE;
}

gboolean
seahorse_service_get_keyset (SeahorseService *svc, gchar *ktype, 
                             gchar **path, GError **error)
{
    if (!g_hash_table_lookup (svc->keysets, ktype)) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                     _("Invalid or unrecognized key type: %s"), ktype);
        return FALSE;
    }
    
    *path = g_strdup_printf (KEYSET_PATH, ktype);
    return TRUE;
}

gboolean
seahorse_service_has_key_field (SeahorseService *svc, gchar *key, gchar *field,
                                gboolean *has, GError **error)
{
    SeahorseKey *skey;
    GParamSpec *spec;
    
    skey = seahorse_service_key_from_dbus (key);
    if (!skey) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
					 _("Invalid or unrecognized key: %s"), key);
        return FALSE;
    }
    
    spec = g_object_class_find_property (G_OBJECT_GET_CLASS (skey), field);
    *has = spec != NULL ? TRUE : FALSE;
    return TRUE; 
}

gboolean
seahorse_service_get_key_field (SeahorseService *svc, gchar *key, gchar *field,
                                GValue *value, GError **error)
{
    SeahorseKey *skey;
    GParamSpec *spec;

    skey = seahorse_service_key_from_dbus (key);
    if (!skey) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
					 _("Invalid or unrecognized key: %s"), key);
        return FALSE;
    }
    
    spec = g_object_class_find_property (G_OBJECT_GET_CLASS (skey), field);
    if (!spec) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
					 _("Invalid field: %s"), field);
        return FALSE;
    }    

    g_value_init (value, spec->value_type);
    g_object_get_property (G_OBJECT (skey), field, value);
    return TRUE; 
}

gboolean
seahorse_service_get_key_fields (SeahorseService *svc, gchar *key, gchar **fields,
                                 GHashTable **values, GError **error)
{
    SeahorseKey *skey;
    GParamSpec *spec;
    GValue *value;
    
    skey = seahorse_service_key_from_dbus (key);
    if (!skey) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
					 _("Invalid or unrecognized key: %s"), key);
        return FALSE;
    }

    /* TODO: Free the value properly */    
    *values = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, value_free);
    
    while (*fields) {

        spec = g_object_class_find_property (G_OBJECT_GET_CLASS (skey), *fields);
        if (spec) {
            value = g_new0 (GValue, 1);
            g_value_init (value, spec->value_type);
            g_object_get_property (G_OBJECT (skey), *fields, value);
            g_hash_table_insert (*values, g_strdup (*fields), value);
            value = NULL;
        }
        
        fields++;
    }
    
    return TRUE; 
}

gboolean
seahorse_service_get_key_names (SeahorseService *svc, gchar *key, gchar ***names,
                                gchar ***cns, GError **error)
{
    SeahorseKey *skey;
    GArray *anames;
    GArray *acns;
    guint i, n;
    gchar *t;
    
    skey = seahorse_service_key_from_dbus (key);
    if (!skey) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
					 _("Invalid or unrecognized key: %s"), key);
        return FALSE;
    }
    
    anames = g_array_new (TRUE, TRUE, sizeof (gchar*));
    acns = g_array_new (TRUE, TRUE, sizeof (gchar*));

    for (i = 0, n = seahorse_key_get_num_names (skey); i < n; i++) {
        if(!(t = seahorse_key_get_name (skey, i)))
            t = g_strdup ("");
        g_array_append_val (anames, t);
        if(!(t = seahorse_key_get_name_cn (skey, i)))
            t = g_strdup ("");
        g_array_append_val (acns, t);
    }
    
    *names = (gchar**)g_array_free (anames, FALSE);
    *cns = (gchar**)g_array_free (acns, FALSE);
    return TRUE;
}

gboolean
seahorse_service_import_keys (SeahorseService *svc, gchar *ktype, 
                              gchar *data, gchar ***keys, GError **error)
{
    SeahorseKeySource *sksrc;
    SeahorseOperation *op;
    gpgme_data_t gdata;
    gpgme_error_t gerr;
    GArray *a;
    GList *l;
    gchar *t;
    
    sksrc = seahorse_context_find_key_source (SCTX_APP (), g_quark_from_string (ktype), 
                                              SKEY_LOC_LOCAL);
    if (!sksrc) {
		g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
					 _("Invalid or unrecognized key type: %s"), ktype);
        return FALSE;
    }
    
    /* TODO: We should be doing base64 on these */
    gerr = gpgme_data_new_from_mem (&gdata, data, strlen (data), 0);
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_gpgme_to_error (gerr, error);
        return FALSE;
    }
    
    op = seahorse_key_source_import (sksrc, gdata);
    seahorse_operation_wait (op);
    
    a = g_array_new (TRUE, TRUE, sizeof (gchar*));
    for (l = (GList*)seahorse_operation_get_result (op); l; l = g_list_next (l)) {
        t = seahorse_service_key_to_dbus (SEAHORSE_KEY (l->data));
        g_array_append_val (a, t);
    }
    
    *keys = (gchar**)g_array_free (a, FALSE);
    
    g_object_unref (op);
    return TRUE;
}

gboolean
seahorse_service_export_keys (SeahorseService *svc, gchar *ktype,
                              gchar **keys, gchar **data, GError **error)
{
    SeahorseKeySource *sksrc;
    SeahorseOperation *op;
    SeahorseKey *skey;
    gpgme_data_t gdata;
    gpgme_error_t gerr;
    GList *next;
    GList *l = NULL;
    GQuark type;
    
    type = g_quark_from_string (ktype);
    
    gerr = gpgme_data_new (&gdata);
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_gpgme_to_error (gerr, error);
        return FALSE;
    }    
    
    while (*keys) {
        skey = seahorse_service_key_from_dbus (*keys);
        
        if (!skey || seahorse_key_get_ktype (skey) != type) {
            gpgme_data_release (gdata);
		    g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
			    		 _("Invalid or unrecognized key: %s"), *keys);
            return FALSE;
        }
        
        l = g_list_prepend (l, skey);
        keys++;
    }    

    /* Sort by key source */
    l = seahorse_util_keylist_sort (l);
    
    while (l) {
     
        /* Break off one set (same keysource) */
        next = seahorse_util_keylist_splice (l);
        
        skey = SEAHORSE_KEY (l->data);

        /* Export from this key source */        
        sksrc = seahorse_key_get_source (skey);
        g_return_val_if_fail (sksrc != NULL, FALSE);
        
        /* We pass our own data object, to which data is appended */
        op = seahorse_key_source_export (sksrc, l, FALSE, gdata);
        g_return_val_if_fail (op != NULL, FALSE);

        g_list_free (l);
        l = next;
        
        seahorse_operation_wait (op);
    
        if (!seahorse_operation_is_successful (op)) {

            /* Ignore the rest, break loop */
            g_list_free (l);
            
            seahorse_operation_steal_error (op, error);
            g_object_unref (op);
            
            gpgme_data_release (gdata);
            return FALSE;
        }        
        
        g_object_unref (op);
    } 
    
    /* TODO: We should be base64 encoding this */
    *data = seahorse_util_write_data_to_text (gdata, TRUE);
    return TRUE;
}

gboolean
seahorse_service_match_keys (SeahorseService *svc, gchar *ktype, gint flags, 
                             gchar **patterns, gchar ***keys, gchar***unmatched,
                             GError **error)
{
    /* TODO: Implement match keys */
    g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_NOTIMPLEMENTED, "TODO");
    return FALSE;    
    
}

gboolean
seahorse_service_match_save (SeahorseService *svc, gchar *ktype, gint flags, 
                             gchar **patterns, gchar **keys, GError **error)
{
    /* TODO: Implement match keys */
    g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_NOTIMPLEMENTED, "TODO");
    return FALSE;    
}

gboolean
seahorse_service_discover_keys (SeahorseService *svc, gchar *ktype, gint flags, 
                                gchar **patterns, gchar **keys, GError **error)
{
    /* TODO: Implement discover keys */
    g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_NOTIMPLEMENTED, "TODO");
    return FALSE;    
}

/* -----------------------------------------------------------------------------
 * SIGNAL HANDLERS 
 */

static void
seahorse_service_added (SeahorseContext *sctx, SeahorseKey *skey, SeahorseService *svc)
{
    GQuark ktype = seahorse_key_get_ktype (skey);
    const gchar *keytype = g_quark_to_string (ktype);
    gchar *dbus_id;
    
    /* Check if we have a keyset for this key type, and add if not */
    if (svc->keysets && !g_hash_table_lookup (svc->keysets, keytype)) {
        SeahorseKeyset *keyset = seahorse_service_keyset_new (ktype);
                
        /* Register it with DBUS */
        dbus_id = g_strdup_printf (KEYSET_PATH, keytype);
        dbus_g_connection_register_g_object (seahorse_dbus_server_get_connection (),
                                             dbus_id, G_OBJECT (keyset));    
        g_free (dbus_id);
        
        g_hash_table_replace (svc->keysets, g_strdup (keytype), keyset);
    }
}

static void
seahorse_service_changed (SeahorseContext *sctx, SeahorseKey *skey, 
                          SeahorseKeyChange change, SeahorseService *svc)
{
    /* Do the same thing as when a key is added */
    seahorse_service_added (sctx, skey, svc);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_service_dispose (GObject *gobject)
{
    SeahorseService *svc = SEAHORSE_SERVICE (gobject);
    
    if (svc->keysets)
        g_hash_table_destroy (svc->keysets);
    svc->keysets = NULL;
    
    g_signal_handlers_disconnect_by_func (SCTX_APP (), seahorse_service_added, svc);
    g_signal_handlers_disconnect_by_func (SCTX_APP (), seahorse_service_changed, svc);
    
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}
    
static void
seahorse_service_class_init (SeahorseServiceClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_service_dispose;
}

static void
seahorse_service_init (SeahorseService *svc)
{
    GList *keys, *l;
    
    /* We keep around a keyset for each keytype */
    svc->keysets = g_hash_table_new_full (g_str_hash, g_str_equal, 
                                          g_free, g_object_unref);
    
    /* Fill in keysets for any keys already in the context */
    keys = seahorse_context_get_keys (SCTX_APP (), NULL);
    for (l = keys; l; l = g_list_next (l)) 
        seahorse_service_added (SCTX_APP (), SEAHORSE_KEY (l->data), svc);
    g_list_free (keys);
    
    /* And now listen for new key types */
    g_signal_connect (SCTX_APP (), "added", G_CALLBACK (seahorse_service_added), svc);
    g_signal_connect (SCTX_APP (), "changed", G_CALLBACK (seahorse_service_changed), svc);
}
