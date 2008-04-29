/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>

#include "seahorse-gkeyring-source.h"
#include "seahorse-operation.h"
#include "seahorse-util.h"
#include "seahorse-gkeyring-item.h"
#include "seahorse-secure-memory.h"
#include "seahorse-passphrase.h"
#include "seahorse-gkeyring-operation.h"

#include "common/seahorse-registry.h"

#include <gnome-keyring.h>

/* Override the DEBUG_REFRESH_ENABLE switch here */
#define DEBUG_REFRESH_ENABLE 0

#ifndef DEBUG_REFRESH_ENABLE
#if _DEBUG
#define DEBUG_REFRESH_ENABLE 1
#else
#define DEBUG_REFRESH_ENABLE 0
#endif
#endif

#if DEBUG_REFRESH_ENABLE
#define DEBUG_REFRESH(x)    g_printerr(x)
#else
#define DEBUG_REFRESH(x)
#endif

enum {
    PROP_0,
    PROP_KEYRING_NAME,
    PROP_KEY_TYPE,
    PROP_KEY_DESC,
    PROP_LOCATION
};

struct _SeahorseGKeyringSourcePrivate {
    SeahorseMultiOperation *operations;     /* A list of all current operations */    
    gchar *keyring_name;                    /* The key ring name */
};

G_DEFINE_TYPE (SeahorseGKeyringSource, seahorse_gkeyring_source, SEAHORSE_TYPE_KEY_SOURCE);

/* Forward decls */

static void         key_changed             (SeahorseKey *skey, SeahorseKeyChange change, 
                                             SeahorseKeySource *sksrc);

static void         key_destroyed           (SeahorseKey *skey, SeahorseKeySource *sksrc);

/* -----------------------------------------------------------------------------
 * LIST OPERATION 
 */
 

#define SEAHORSE_TYPE_LIST_OPERATION            (seahorse_list_operation_get_type ())
#define SEAHORSE_LIST_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperation))
#define SEAHORSE_LIST_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperationClass))
#define SEAHORSE_IS_LIST_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_LIST_OPERATION))
#define SEAHORSE_IS_LIST_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_LIST_OPERATION))
#define SEAHORSE_LIST_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperationClass))

enum {
    PART_INFO = 0x01,
    PART_ATTRS = 0x02,
    PART_ACL = 0x04
};

DECLARE_OPERATION (List, list)

    SeahorseGKeyringSource *gsrc;

    GList *remaining;
    guint total;
    
    guint32 current_parts;
    guint32 current_id;
    guint32 info_flags;
    GnomeKeyringItemInfo *current_info;
    GnomeKeyringAttributeList *current_attrs;
    GList *current_acl;
    
    gpointer request;
    gpointer request_attrs;
    gpointer request_acl;
    
    GHashTable *checks;

END_DECLARE_OPERATION

IMPLEMENT_OPERATION (List, list)

static void     process_next_item       (SeahorseListOperation *lop);

static guint32
parse_keyid (GQuark keyid)
{
    const gchar *string;
    const gchar *num;
    gint ret;
    char *t;
    
    string = g_quark_to_string (keyid);
    if (!string)
        return 0;
    
    num = strchr (string, ':');
    if (!num)
        return 0;
    
    ret = strtol (num + 1, &t, 16);
    if (ret < 0 || *t)
        return 0;
    
    return (guint32)ret;
}

static void
keyring_operation_failed (SeahorseListOperation *lop, GnomeKeyringResult result)
{
    SeahorseOperation *op = SEAHORSE_OPERATION (lop);
    GError *err = NULL;
    
    g_assert (result != GNOME_KEYRING_RESULT_OK);
    g_assert (result != GNOME_KEYRING_RESULT_CANCELLED);
    
    if (!seahorse_operation_is_running (op))
        return;
    
    if (lop->request)
        gnome_keyring_cancel_request (lop->request);
    lop->request = NULL;

    if (lop->request_attrs)
        gnome_keyring_cancel_request (lop->request_attrs);
    lop->request_attrs = NULL;

    if (lop->request_acl)
        gnome_keyring_cancel_request (lop->request_acl);
    lop->request_acl = NULL;
    
    seahorse_gkeyring_operation_parse_error (result, &err);
    g_assert (err != NULL);
    
    seahorse_operation_mark_done (op, FALSE, err);
}

static gboolean
have_complete_item (SeahorseListOperation *lop)
{
    SeahorseGKeyringItem *git = NULL;
    SeahorseGKeyringItem *prev;
    GQuark keyid;
    
    g_assert (lop->current_id);
    
    if (lop->current_parts != (PART_INFO | PART_ATTRS | PART_ACL))
        return FALSE;
    
    g_assert (lop->current_info);
  
    keyid = seahorse_gkeyring_item_get_cannonical (lop->current_id);
    g_return_val_if_fail (keyid, FALSE);
    
    /* Mark this key as seen */
    if (lop->checks)
        g_hash_table_remove (lop->checks, GUINT_TO_POINTER (keyid));
    
    g_assert (SEAHORSE_IS_GKEYRING_SOURCE (lop->gsrc));
    prev = SEAHORSE_GKEYRING_ITEM (seahorse_context_get_key (SCTX_APP (), 
                                   SEAHORSE_KEY_SOURCE (lop->gsrc), keyid));

    /* Check if we can just replace the key on the object */
    if (prev != NULL) {
        g_object_set (prev, "item-info", lop->current_info, NULL);
        if (lop->current_attrs)
            g_object_set (prev, "item-attributes", lop->current_attrs, NULL);
        g_object_set (prev, "item-acl", lop->current_acl, NULL);
		
        lop->current_info = NULL;
        lop->current_acl = NULL;
        lop->current_attrs = NULL;
        return TRUE;
    }
    
    git = seahorse_gkeyring_item_new (SEAHORSE_KEY_SOURCE (lop->gsrc), lop->current_id, 
                                      lop->current_info, lop->current_attrs, lop->current_acl);
 
    /* We listen in to get notified of changes on this key */
    g_signal_connect (git, "changed", G_CALLBACK (key_changed), SEAHORSE_KEY_SOURCE (lop->gsrc));
    g_signal_connect (git, "destroy", G_CALLBACK (key_destroyed), SEAHORSE_KEY_SOURCE (lop->gsrc));

    /* Add to context */ 
    seahorse_context_take_key (SCTX_APP (), SEAHORSE_KEY (git));

    lop->current_info = NULL;
    lop->current_acl = NULL;
    lop->current_attrs = NULL;
    return TRUE;
}

static void 
item_info_ready (GnomeKeyringResult result, GnomeKeyringItemInfo *info, 
                 SeahorseListOperation *lop)
{
    if (result == GNOME_KEYRING_RESULT_CANCELLED)
        return;
    
    lop->request = NULL;

    if (result != GNOME_KEYRING_RESULT_OK) {
        keyring_operation_failed (lop, result);
        return;
    }
    
    g_assert (lop->current_id);
    g_assert (!lop->current_info);
    
    lop->current_info = gnome_keyring_item_info_copy (info);
    lop->current_parts |= PART_INFO;
    
    if (have_complete_item (lop))
        process_next_item (lop);
}

static void 
item_attrs_ready (GnomeKeyringResult result, GnomeKeyringAttributeList *attributes, 
                 SeahorseListOperation *lop)
{
    if (result == GNOME_KEYRING_RESULT_CANCELLED)
        return;
    
    lop->request_attrs = NULL;

    if (result != GNOME_KEYRING_RESULT_OK) {
        keyring_operation_failed (lop, result);
        return;
    }
    
    g_assert (lop->current_id);
    g_assert (!lop->current_attrs);
    
    lop->current_attrs = gnome_keyring_attribute_list_copy (attributes);
    lop->current_parts |= PART_ATTRS;
    
    if (have_complete_item (lop))
        process_next_item (lop);
}

static void 
item_acl_ready (GnomeKeyringResult result, GList *acl, SeahorseListOperation *lop)
{
    if (result == GNOME_KEYRING_RESULT_CANCELLED)
        return;
    
    lop->request_acl = NULL;

    if (result != GNOME_KEYRING_RESULT_OK) {
        keyring_operation_failed (lop, result);
        return;
    }
    
    g_assert (lop->current_id);
    g_assert (!lop->current_acl);
    
    lop->current_acl = gnome_keyring_acl_copy (acl);
    lop->current_parts |= PART_ACL;
    
    if (have_complete_item (lop))
        process_next_item (lop);
}

/* Remove the given key from the context */
static void
remove_key_from_context (gpointer kt, SeahorseKey *dummy, SeahorseKeySource *sksrc)
{
    /* This function gets called as a GHFunc on the lop->checks hashtable. */
    GQuark keyid = GPOINTER_TO_UINT (kt);
    SeahorseKey *skey;
    
    skey = seahorse_context_get_key (SCTX_APP (), sksrc, keyid);
    if (skey != NULL)
        seahorse_context_remove_key (SCTX_APP (), skey);
}

static void
process_next_item (SeahorseListOperation *lop)
{
    g_assert (lop);
    g_assert (SEAHORSE_IS_GKEYRING_SOURCE (lop->gsrc));

    seahorse_operation_mark_progress_full (SEAHORSE_OPERATION (lop), NULL, 
                                           lop->total - g_list_length (lop->remaining), 
                                           lop->total);
    
   lop->current_id = 0;
   lop->current_parts = 0;
   g_assert (!lop->current_info);
   g_assert (!lop->current_attrs);
   g_assert (!lop->current_acl);
    
    /* Check if we're done */
    if (g_list_length (lop->remaining) == 0) {
        
        if (lop->checks)
            g_hash_table_foreach (lop->checks, (GHFunc)remove_key_from_context, 
                                  SEAHORSE_KEY_SOURCE (lop->gsrc));
        
        seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, NULL);
        return;
    }
    
    /* Start the next item */
    g_assert (lop->current_id == 0);
    lop->current_id = GPOINTER_TO_UINT (lop->remaining->data);
    lop->remaining = g_list_delete_link (lop->remaining, lop->remaining);
    
    g_assert (!lop->request);

    /* The various callbacks agree on when the item is fully loaded */
    lop->request = gnome_keyring_item_get_info_full (lop->gsrc->pv->keyring_name, 
                                lop->current_id, lop->info_flags, 
                                (GnomeKeyringOperationGetItemInfoCallback)item_info_ready, lop, NULL);

    lop->request_attrs = gnome_keyring_item_get_attributes (lop->gsrc->pv->keyring_name, lop->current_id, 
                                (GnomeKeyringOperationGetAttributesCallback)item_attrs_ready, lop, NULL);
	                            
    lop->request_acl = gnome_keyring_item_get_acl (lop->gsrc->pv->keyring_name, lop->current_id, 
                                (GnomeKeyringOperationGetListCallback)item_acl_ready, lop, NULL);
}

static void 
keyring_ids_ready (GnomeKeyringResult result, GList *list, SeahorseListOperation *lop)
{
    if (result == GNOME_KEYRING_RESULT_CANCELLED)
        return;
    
    lop->request = NULL;

    if (result != GNOME_KEYRING_RESULT_OK) {
        keyring_operation_failed (lop, result);
        return;
    }
    
    g_assert (lop->remaining == NULL);
    lop->remaining = g_list_copy (list);
    lop->total = g_list_length (list);
    
    process_next_item (lop);
}

static SeahorseListOperation*
start_list_operation (SeahorseGKeyringSource *gsrc, GQuark keyid)
{
    SeahorseListOperation *lop;
    GList *keys, *l;

    g_assert (SEAHORSE_IS_GKEYRING_SOURCE (gsrc));

    lop = g_object_new (SEAHORSE_TYPE_LIST_OPERATION, NULL);
    lop->gsrc = gsrc;
    
    seahorse_operation_mark_start (SEAHORSE_OPERATION (lop));
    
    /* When we know which key to load go directly to step two */
    if (keyid) {
        guint32 id;
        
        id = parse_keyid (keyid);
        g_return_val_if_fail (id != 0, NULL);
        
        lop->remaining = g_list_prepend (lop->remaining, GUINT_TO_POINTER (id));
        lop->total = 1;
        
        /* Load everything including the secret */
        lop->info_flags = GNOME_KEYRING_ITEM_INFO_ALL;
        
        seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), _("Retrieving key"), -1);
        process_next_item (lop);
        return lop;
    }
    
    /* When loading new keys prepare a list of current */
    lop->checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
    keys = seahorse_context_get_keys (SCTX_APP (), SEAHORSE_KEY_SOURCE (gsrc));
    for (l = keys; l; l = g_list_next (l))
        g_hash_table_insert (lop->checks, GUINT_TO_POINTER (seahorse_key_get_keyid (l->data)), 
                             GUINT_TO_POINTER (TRUE));
    g_list_free (keys);
    
    /* Start listing of ids */
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), _("Listing passwords"), -1);
    lop->request = gnome_keyring_list_item_ids (gsrc->pv->keyring_name, 
                    (GnomeKeyringOperationGetListCallback)keyring_ids_ready, lop, NULL);
    
    return lop;
}

static void 
seahorse_list_operation_init (SeahorseListOperation *lop)
{
    /* Everything already set to zero */
}

static void 
seahorse_list_operation_dispose (GObject *gobject)
{
    SeahorseListOperation *lop = SEAHORSE_LIST_OPERATION (gobject);
    
    /* Cancel it if it's still running */
    if (seahorse_operation_is_running (SEAHORSE_OPERATION (lop)))
        seahorse_list_operation_cancel (SEAHORSE_OPERATION (lop));
    g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (lop)));
    
    /* The above cancel should have stopped this */
    g_assert (lop->request == NULL);

    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_list_operation_finalize (GObject *gobject)
{
    SeahorseListOperation *lop = SEAHORSE_LIST_OPERATION (gobject);
    g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (lop)));
    
    if (lop->remaining)
        g_list_free (lop->remaining);
    lop->remaining = NULL;
    
    if (lop->current_info)
        gnome_keyring_item_info_free (lop->current_info);
    lop->current_info = NULL;
    if (lop->current_attrs)
        gnome_keyring_attribute_list_free (lop->current_attrs);
    lop->current_attrs = NULL;
    if (lop->current_acl)
        gnome_keyring_acl_free (lop->current_acl);
    lop->current_acl = NULL;
    
    if (lop->checks)
        g_hash_table_destroy (lop->checks);
    lop->checks = NULL;
    
    /* The above cancel should have stopped this */
    g_assert (lop->request == NULL);

    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_list_operation_cancel (SeahorseOperation *operation)
{
    SeahorseListOperation *lop = SEAHORSE_LIST_OPERATION (operation);    

    if (lop->request)
        gnome_keyring_cancel_request (lop->request);
    lop->request = NULL;
    
    if (seahorse_operation_is_running (operation))
        seahorse_operation_mark_done (operation, TRUE, NULL);
}

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseKeySource *sksrc)
{
    /* TODO: We need to fix these key change flags. Currently the only thing
     * that sets 'ALL' is when we replace a key in an skey. We don't 
     * need to reload in that case */
    
    if (change == SKEY_CHANGE_ALL)
        return;

    seahorse_key_source_load_async (sksrc, seahorse_key_get_keyid (skey));
}

static void
key_destroyed (SeahorseKey *skey, SeahorseKeySource *sksrc)
{
    g_signal_handlers_disconnect_by_func (skey, key_changed, sksrc);
    g_signal_handlers_disconnect_by_func (skey, key_destroyed, sksrc);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_gkeyring_source_init (SeahorseGKeyringSource *gsrc)
{
    /* init private vars */
    gsrc->pv = g_new0 (SeahorseGKeyringSourcePrivate, 1);
    gsrc->pv->operations = seahorse_multi_operation_new ();
}

static GObject*  
seahorse_gkeyring_source_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
    GObject* obj = G_OBJECT_CLASS (seahorse_gkeyring_source_parent_class)->constructor (type, n_props, props);
    return obj;
}

static void 
seahorse_gkeyring_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                       GParamSpec *pspec)
{
    SeahorseGKeyringSource *gsrc = SEAHORSE_GKEYRING_SOURCE (object);

    switch (prop_id) {
    case PROP_KEYRING_NAME:
        g_value_set_string (value, gsrc->pv->keyring_name);
        break;
    case PROP_KEY_TYPE:
        g_value_set_uint (value, SEAHORSE_GKR);
        break;
    case PROP_KEY_DESC:
        g_value_set_string (value, _("Password"));
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, SKEY_LOC_LOCAL);
        break;
    }
}

static void 
seahorse_gkeyring_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                       GParamSpec *pspec)
{
    SeahorseGKeyringSource *gsrc = SEAHORSE_GKEYRING_SOURCE (object);

    switch (prop_id) {
    case PROP_KEYRING_NAME:
        if (g_value_get_string (value) != gsrc->pv->keyring_name) {
            g_free (gsrc->pv->keyring_name);
            gsrc->pv->keyring_name = g_strdup (g_value_get_string (value));
        }
        break;
    };
}

static SeahorseOperation*
seahorse_gkeyring_source_load (SeahorseKeySource *src, GQuark keyid)
{
    SeahorseGKeyringSource *gsrc;
    SeahorseListOperation *lop;
    
    g_assert (SEAHORSE_IS_KEY_SOURCE (src));
    gsrc = SEAHORSE_GKEYRING_SOURCE (src);
    
    lop = start_list_operation (gsrc, keyid);
    seahorse_multi_operation_take (gsrc->pv->operations, SEAHORSE_OPERATION (lop));
    
    g_object_ref (lop);
    return SEAHORSE_OPERATION (lop);
}

static void
seahorse_gkeyring_source_stop (SeahorseKeySource *src)
{
    SeahorseGKeyringSource *gsrc;
    
    g_assert (SEAHORSE_IS_KEY_SOURCE (src));
    gsrc = SEAHORSE_GKEYRING_SOURCE (src);
    
    if(seahorse_operation_is_running (SEAHORSE_OPERATION (gsrc->pv->operations)))
        seahorse_operation_cancel (SEAHORSE_OPERATION (gsrc->pv->operations));
}

static guint
seahorse_gkeyring_source_get_state (SeahorseKeySource *src)
{
    SeahorseGKeyringSource *gsrc;
    
    g_assert (SEAHORSE_IS_KEY_SOURCE (src));
    gsrc = SEAHORSE_GKEYRING_SOURCE (src);
    
    return seahorse_operation_is_running (SEAHORSE_OPERATION (gsrc->pv->operations)) ? 
                                          SKSRC_LOADING : 0;
}


static SeahorseOperation* 
seahorse_gkeyring_source_import (SeahorseKeySource *sksrc, GInputStream *input)
{
	GError *err = NULL;

	g_return_val_if_fail (SEAHORSE_IS_GKEYRING_SOURCE (sksrc), NULL);
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);

	/* TODO: Implement properly */
	g_set_error (&err, 0, 0, "gnome-keyring import support not implemented");
	return seahorse_operation_new_complete (err);   
}

static SeahorseOperation* 
seahorse_gkeyring_source_export (SeahorseKeySource *sksrc, GList *keys, 
                                 gboolean complete, GOutputStream *output)
{
	GError *err = NULL;
    
	g_return_val_if_fail (SEAHORSE_IS_GKEYRING_SOURCE (sksrc), NULL);
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
    
	/* TODO: Implement properly */
	g_set_error (&err, 0, 0, "gnome-keyring export support not implemented");
	return seahorse_operation_new_complete (err);    
}

static gboolean            
seahorse_gkeyring_source_remove (SeahorseKeySource *sksrc, SeahorseKey *skey,
                                 guint name, GError **err)
{
    SeahorseGKeyringItem *git;
    SeahorseGKeyringSource *gsrc;
    GnomeKeyringResult res;
    gboolean ret;
    
    g_return_val_if_fail (SEAHORSE_IS_GKEYRING_ITEM (skey), FALSE);
    git = SEAHORSE_GKEYRING_ITEM (skey);

    g_return_val_if_fail (SEAHORSE_IS_GKEYRING_SOURCE (sksrc), FALSE);
    gsrc = SEAHORSE_GKEYRING_SOURCE (sksrc);

    /* Don't have multiple names */
    g_return_val_if_fail (name == 0, FALSE);
   
    res = gnome_keyring_item_delete_sync (gsrc->pv->keyring_name, git->item_id);
    ret = seahorse_gkeyring_operation_parse_error (res, err);
    
    if (ret) {
        seahorse_context_remove_key (SCTX_APP (), skey);
        seahorse_key_source_load_async (sksrc, 0);
    }
    
    return ret;
}

static void
seahorse_gkeyring_source_dispose (GObject *gobject)
{
    SeahorseGKeyringSource *gsrc;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    gsrc = SEAHORSE_GKEYRING_SOURCE (gobject);
    g_assert (gsrc->pv);
    
    /* Clear out all operations */
    if (gsrc->pv->operations) {
        if(seahorse_operation_is_running (SEAHORSE_OPERATION (gsrc->pv->operations)))
            seahorse_operation_cancel (SEAHORSE_OPERATION (gsrc->pv->operations));
        g_object_unref (gsrc->pv->operations);
        gsrc->pv->operations = NULL;
    }

    G_OBJECT_CLASS (seahorse_gkeyring_source_parent_class)->dispose (gobject);
}

static void
seahorse_gkeyring_source_finalize (GObject *gobject)
{
    SeahorseGKeyringSource *gsrc;
  
    gsrc = SEAHORSE_GKEYRING_SOURCE (gobject);
    g_assert (gsrc->pv);
    
    g_assert (gsrc->pv->operations == NULL);
    
    g_free (gsrc->pv->keyring_name);
    g_free (gsrc->pv);
    
    G_OBJECT_CLASS (seahorse_gkeyring_source_parent_class)->finalize (gobject);
}

static void
seahorse_gkeyring_source_class_init (SeahorseGKeyringSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeySourceClass *key_class;
    
    seahorse_gkeyring_source_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->constructor = seahorse_gkeyring_source_constructor;
    gobject_class->dispose = seahorse_gkeyring_source_dispose;
    gobject_class->finalize = seahorse_gkeyring_source_finalize;
    gobject_class->set_property = seahorse_gkeyring_source_set_property;
    gobject_class->get_property = seahorse_gkeyring_source_get_property;
    
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);    
    key_class->load = seahorse_gkeyring_source_load;
    key_class->stop = seahorse_gkeyring_source_stop;
    key_class->get_state = seahorse_gkeyring_source_get_state;
    key_class->import = seahorse_gkeyring_source_import;
    key_class->export = seahorse_gkeyring_source_export;
    key_class->remove = seahorse_gkeyring_source_remove;

    g_object_class_install_property (gobject_class, PROP_KEYRING_NAME,
        g_param_spec_string ("keyring-name", "Keyring Name", "GNOME Keyring name",
                             NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
    g_object_class_install_property (gobject_class, PROP_KEY_TYPE,
        g_param_spec_uint ("key-type", "Key Type", "Key type that originates from this key source.", 
                           0, G_MAXUINT, SKEY_UNKNOWN, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_KEY_DESC,
        g_param_spec_string ("key-desc", "Key Desc", "Description for keys that originate here.",
                             NULL, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOCATION,
        g_param_spec_uint ("location", "Key Location", "Where the key is stored. See SeahorseKeyLoc", 
                           0, G_MAXUINT, SKEY_LOC_INVALID, G_PARAM_READABLE));    
    
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_GKEYRING_SOURCE, "key-source", "local", SEAHORSE_GKR_STR, NULL);
}

/* -------------------------------------------------------------------------- 
 * PUBLIC
 */

SeahorseGKeyringSource*
seahorse_gkeyring_source_new (const gchar *keyring_name)
{
   return g_object_new (SEAHORSE_TYPE_GKEYRING_SOURCE, "keyring-name", keyring_name, NULL);
}   

const gchar*
seahorse_gkeyring_source_get_keyring_name (SeahorseGKeyringSource *gsrc)
{
    return gsrc->pv->keyring_name;
}
