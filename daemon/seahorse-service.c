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

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <dbus/dbus-glib-bindings.h>

#include "seahorse-context.h"
#include "seahorse-daemon.h"
#include "seahorse-libdialogs.h"
#include "seahorse-object.h"
#include "seahorse-service.h"
#include "seahorse-source.h"
#include "seahorse-util.h"

#include <gio/gio.h>

#define KEYSET_PATH "/org/gnome/seahorse/keys/%s"
#define KEYSET_PATH_LOCAL "/org/gnome/seahorse/keys/%s/local"

G_DEFINE_TYPE (SeahorseService, seahorse_service, G_TYPE_OBJECT);

/**
 * SECTION:seahorse-service
 * @short_description: Seahorse service DBus methods. The other DBus methods can
 * be found in other files
 * @include:seahorse-service.h
 *
 **/

/**
* type: a string (gchar)
* dummy: not used
* a: an array (GArray)
*
* Adds a copy of "type" to "a"
*
*/
static void
copy_to_array (const gchar *type, gpointer dummy, GArray *a)
{
    gchar *v = g_strdup (type);
    g_array_append_val (a, v);
}

/**
* svc: the seahorse context
* ktype: the key source to add
*
* Adds DBus ids for the keysets. The keysets "ktype" of the service will be set to the
* local keyset.
*
*/
static void 
add_key_source (SeahorseService *svc, GQuark ktype)
{
    const gchar *keytype = g_quark_to_string (ktype);
    SeahorseSet *keyset;
    gchar *dbus_id;
    
    /* Check if we have a keyset for this key type, and add if not */
    if (svc->keysets && !g_hash_table_lookup (svc->keysets, keytype)) {

        /* Keyset for all keys */
        keyset = seahorse_service_keyset_new (ktype, SEAHORSE_LOCATION_INVALID);
        dbus_id = g_strdup_printf (KEYSET_PATH, keytype);
        dbus_g_connection_register_g_object (seahorse_dbus_server_get_connection (),
                                             dbus_id, G_OBJECT (keyset));    
        g_free (dbus_id);
        
        /* Keyset for local keys */
        keyset = seahorse_service_keyset_new (ktype, SEAHORSE_LOCATION_LOCAL);
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

/**
* seahorse_service_get_key_types:
* @svc: the seahorse context
* @ret: the supported keytypes of seahorse
* @error: an error var, not used
*
* DBus: GetKeyTypes
*
* Returns all available keytypes
*
* Returns: True
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

/**
* seahorse_service_get_keyset:
* @svc: the seahorse context
* @ktype: the type of the key
* @path: the path to the keyset
* @error: set if the key type was not found
*
* DBus: GetKeyset
*
* Returns: False if there is no matching keyset, True else
*/
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

/**
* seahorse_service_import_keys:
* @svc: the seahorse context
* @ktype: the keytype (example: "openpgp")
* @data: ASCII armored key data (one or more keys)
* @keys: the keys that have been imorted (out)
* @error: the error
*
* DBus: ImportKeys
*
* Imports a buffer containing one or more keys. Returns the keyids
*
* Returns: True on success
*/
gboolean
seahorse_service_import_keys (SeahorseService *svc, gchar *ktype, 
                              gchar *data, gchar ***keys, GError **error)
{
    SeahorseSource *sksrc;
    SeahorseOperation *op;
    GInputStream *input;
    GArray *a;
    GList *l;
    gchar *t;
    guint keynum = 0;
    
    sksrc = seahorse_context_find_source (SCTX_APP (), g_quark_from_string (ktype), 
                                          SEAHORSE_LOCATION_LOCAL);
    if (!sksrc) {
        g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                     _("Invalid or unrecognized key type: %s"), ktype);
        return FALSE;
    }
    
	/* TODO: We should be doing base64 on these */
	input = g_memory_input_stream_new_from_data (data, strlen (data), NULL);
	g_return_val_if_fail (input, FALSE);
    
    op = seahorse_source_import (sksrc, G_INPUT_STREAM (input));
    seahorse_operation_wait (op);
    
    a = g_array_new (TRUE, TRUE, sizeof (gchar*));
    for (l = (GList*)seahorse_operation_get_result (op); l; l = g_list_next (l)) {
        t = seahorse_context_id_to_dbus (SCTX_APP (), 
                                seahorse_object_get_id (SEAHORSE_OBJECT (l->data)));
        g_array_append_val (a, t);
        keynum = keynum + 1;
    }
        
    *keys = (gchar**)g_array_free (a, FALSE);
    
	if (keynum > 0)
		seahorse_notify_import (keynum, *keys);
    
	g_object_unref (op);
	g_object_unref (input);

	return TRUE;
}

/**
* seahorse_service_export_keys:
* @svc: the seahorse context
* @ktype: the keytype (example: "openpgp")
* @keys: the keys to export (keyids)
* @data: ASCII armored key data (one or more keys) (out)
* @error: the error
*
* DBus: ExportKeys
*
* Exports keys. Keys to export are defined by keyid. The result is a buffer
* containing ASCII armored keys
*
* Returns: True on success
*/
gboolean
seahorse_service_export_keys (SeahorseService *svc, gchar *ktype,
                              gchar **keys, gchar **data, GError **error)
{
    SeahorseSource *sksrc;
    SeahorseOperation *op;
    SeahorseObject *sobj;
    GMemoryOutputStream *output;
    GList *next;
    GList *l = NULL;
    GQuark type;
    
    type = g_quark_from_string (ktype);
    
    while (*keys) {
        sobj = seahorse_context_object_from_dbus (SCTX_APP (), *keys);
        
        if (!sobj || seahorse_object_get_tag (sobj) != type) {
            g_set_error (error, SEAHORSE_DBUS_ERROR, SEAHORSE_DBUS_ERROR_INVALID, 
                         _("Invalid or unrecognized key: %s"), *keys);
            return FALSE;
        }
        
        l = g_list_prepend (l, sobj);
        keys++;
    }    

    output = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0, g_realloc, NULL));
    g_return_val_if_fail (output, FALSE);
    
    /* Sort by key source */
    l = seahorse_util_objects_sort (l);
    
    while (l) {
     
        /* Break off one set (same keysource) */
        next = seahorse_util_objects_splice (l);
        
        sobj = SEAHORSE_OBJECT (l->data);

        /* Export from this key source */        
        sksrc = seahorse_object_get_source (sobj);
        g_return_val_if_fail (sksrc != NULL, FALSE);
        
        /* We pass our own data object, to which data is appended */
        op = seahorse_source_export (sksrc, l, G_OUTPUT_STREAM (output));
        g_return_val_if_fail (op != NULL, FALSE);

        g_list_free (l);
        l = next;
        
        seahorse_operation_wait (op);
    
        if (!seahorse_operation_is_successful (op)) {

            /* Ignore the rest, break loop */
            g_list_free (l);
            
            seahorse_operation_copy_error (op, error);
            g_object_unref (op);
            
            g_object_unref (output);
            return FALSE;
        }        
        
        g_object_unref (op);
    } 
    
    /* TODO: We should be base64 encoding this */
    *data = g_memory_output_stream_get_data (output);
    
    /* The above pointer is not freed, because we passed null to g_memory_output_stream_new() */
    g_object_unref (output);
    return TRUE;
}

/**
* seahorse_service_display_notification:
* @svc: the seahorse context
* @heading: the heading of the notification
* @text: the text of the notification
* @icon: the icon of the notification
* @urgent: set to TRUE if the message is urgent
* @error: the error
*
* DBus: DisplayNotification
*
* Displays a notification
*
* Returns: True
*/
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

/**
* sctx: ignored
* sobj: seahorse object
* svc: seahorse service
*
* Handler to update added key sources
*
*/
static void
seahorse_service_added (SeahorseContext *sctx, SeahorseObject *sobj, SeahorseService *svc)
{
    GQuark ktype = seahorse_object_get_tag (sobj);
    add_key_source (svc, ktype);
}

/**
* sctx: ignored
* sobj: seahorse object
* svc: seahorse service
*
* Handler to update changing key sources
*
*/
static void
seahorse_service_changed (SeahorseContext *sctx, SeahorseObject *sobj, SeahorseService *svc)
{
    /* Do the same thing as when a key is added */
    GQuark ktype = seahorse_object_get_tag (sobj);
    add_key_source (svc, ktype);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

/**
* gobject:
*
* Disposes the seahorse dbus service
*
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

/**
* klass: The class to init
*
*
*/
static void
seahorse_service_class_init (SeahorseServiceClass *klass)
{
    GObjectClass *gobject_class;
   
    seahorse_service_parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_service_dispose;
}


/**
* svc:
*
*
* Initialises the service, adds local key sources and connects the signals.
*
*/
static void
seahorse_service_init (SeahorseService *svc)
{
    GSList *srcs, *l;
    
    /* We keep around a keyset for each keytype */
    svc->keysets = g_hash_table_new_full (g_str_hash, g_str_equal, 
                                          g_free, g_object_unref);
    
    /* Fill in keysets for any keys already in the context */
    srcs = seahorse_context_find_sources (SCTX_APP (), SEAHORSE_TAG_INVALID, SEAHORSE_LOCATION_LOCAL);
    for (l = srcs; l; l = g_slist_next (l)) 
        add_key_source (svc, seahorse_source_get_tag (SEAHORSE_SOURCE (l->data)));
    g_slist_free (srcs);
    
    /* And now listen for new key types */
    g_signal_connect (SCTX_APP (), "added", G_CALLBACK (seahorse_service_added), svc);
    g_signal_connect (SCTX_APP (), "changed", G_CALLBACK (seahorse_service_changed), svc);
}
