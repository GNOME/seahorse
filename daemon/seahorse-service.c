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

#include "config.h"
#include <glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "seahorse-daemon.h"
#include "seahorse-service.h"
#include "seahorse-context.h"
#include "seahorse-key-source.h"
#include "seahorse-gpgmex.h"
#include "seahorse-util.h"
#include "seahorse-libdialogs.h"

#define KEYSET_PATH "/org/gnome/seahorse/keys/%s"
#define KEYSET_PATH_LOCAL "/org/gnome/seahorse/keys/%s/local"

G_DEFINE_TYPE (SeahorseService, seahorse_service, G_TYPE_OBJECT);

static void
copy_to_array (const gchar *type, gpointer dummy, GArray *a)
{
    gchar *v = g_strdup (type);
    g_array_append_val (a, v);
}

static void 
add_key_source (SeahorseService *svc, GQuark ktype)
{
    const gchar *keytype = g_quark_to_string (ktype);
    SeahorseKeyset *keyset;
    gchar *dbus_id;
    
    /* Check if we have a keyset for this key type, and add if not */
    if (svc->keysets && !g_hash_table_lookup (svc->keysets, keytype)) {

        /* Keyset for all keys */
        keyset = seahorse_service_keyset_new (ktype, SKEY_LOC_INVALID);
        dbus_id = g_strdup_printf (KEYSET_PATH, keytype);
        dbus_g_connection_register_g_object (seahorse_dbus_server_get_connection (),
                                             dbus_id, G_OBJECT (keyset));    
        g_free (dbus_id);
        
        /* Keyset for local keys */
        keyset = seahorse_service_keyset_new (ktype, SKEY_LOC_LOCAL);
        dbus_id = g_strdup_printf (KEYSET_PATH_LOCAL, keytype);
        dbus_g_connection_register_g_object (seahorse_dbus_server_get_connection (),
                                             dbus_id, G_OBJECT (keyset));    
        g_free (dbus_id);
        
        g_hash_table_replace (svc->keysets, g_strdup (keytype), keyset);
    }
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
    guint keynum = 0;
    
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
        t = seahorse_context_keyid_to_dbus (SCTX_APP (), 
                                seahorse_key_get_keyid (SEAHORSE_KEY (l->data)), 0);
        g_array_append_val (a, t);
        keynum = keynum + 1;
    }
        
    *keys = (gchar**)g_array_free (a, FALSE);
    
    if (keynum > 0)
    seahorse_notify_import (keynum, *keys);
    
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
        skey = seahorse_context_key_from_dbus (SCTX_APP (), *keys, 0);
        
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
            
            seahorse_operation_copy_error (op, error);
            g_object_unref (op);
            
            gpgme_data_release (gdata);
            return FALSE;
        }        
        
        g_object_unref (op);
    } 
    
    /* TODO: We should be base64 encoding this */
    *data = seahorse_util_write_data_to_text (gdata, NULL);
    gpgmex_data_release (gdata);
    return TRUE;
}

gboolean
seahorse_service_display_notification (SeahorseService *svc, gchar *heading,
                                       gchar *text, gchar *icon, gboolean urgent, 
                                       GError **error)
{
    if (!icon || !icon[0])
        icon = NULL;
    
    seahorse_notification_display (heading, text, urgent, icon, NULL);
    return TRUE;
}

#if 0

gboolean
seahorse_service_match_save (SeahorseService *svc, gchar *ktype, gint flags, 
                             gchar **patterns, gchar **keys, GError **error)
{
    /* TODO: Implement match keys */
    g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_NOTIMPLEMENTED, "TODO");
    return FALSE;    
}

#endif /* 0 */

/* -----------------------------------------------------------------------------
 * SIGNAL HANDLERS 
 */

static void
seahorse_service_added (SeahorseContext *sctx, SeahorseKey *skey, SeahorseService *svc)
{
    GQuark ktype = seahorse_key_get_ktype (skey);
    add_key_source (svc, ktype);
}

static void
seahorse_service_changed (SeahorseContext *sctx, SeahorseKey *skey, 
                          SeahorseKeyChange change, SeahorseService *svc)
{
    /* Do the same thing as when a key is added */
    GQuark ktype = seahorse_key_get_ktype (skey);
    add_key_source (svc, ktype);
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
    
    G_OBJECT_CLASS (seahorse_service_parent_class)->dispose (gobject);
}

static void
seahorse_service_class_init (SeahorseServiceClass *klass)
{
    GObjectClass *gobject_class;
   
    seahorse_service_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_service_dispose;
}

static void
seahorse_service_init (SeahorseService *svc)
{
    GSList *srcs, *l;
    
    /* We keep around a keyset for each keytype */
    svc->keysets = g_hash_table_new_full (g_str_hash, g_str_equal, 
                                          g_free, g_object_unref);
    
    /* Fill in keysets for any keys already in the context */
    srcs = seahorse_context_find_key_sources (SCTX_APP (), SKEY_UNKNOWN, SKEY_LOC_LOCAL);
    for (l = srcs; l; l = g_slist_next (l)) 
        add_key_source (svc, seahorse_key_source_get_ktype (SEAHORSE_KEY_SOURCE (l->data)));
    g_slist_free (srcs);
    
    /* And now listen for new key types */
    g_signal_connect (SCTX_APP (), "added", G_CALLBACK (seahorse_service_added), svc);
    g_signal_connect (SCTX_APP (), "changed", G_CALLBACK (seahorse_service_changed), svc);
}
