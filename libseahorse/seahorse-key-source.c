/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Nate Nielsen
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
#include <gnome.h>

#include "seahorse-key-source.h"
#include "seahorse-marshal.h"
#include "seahorse-context.h"
#include "seahorse-pgp-key.h"
#include "seahorse-util.h"

#ifdef WITH_SSH
#include "seahorse-ssh-key.h"
#endif

G_DEFINE_TYPE (SeahorseKeySource, seahorse_key_source, G_TYPE_OBJECT);

/* GObject handlers */
static void seahorse_key_source_dispose    (GObject *gobject);
static void seahorse_key_source_finalize   (GObject *gobject);

static GObjectClass *parent_class = NULL;

static void
seahorse_key_source_class_init (SeahorseKeySourceClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_key_source_dispose;
    gobject_class->finalize = seahorse_key_source_finalize;
}

/* Initialize the object */
static void
seahorse_key_source_init (SeahorseKeySource *sksrc)
{

}

/* dispose of all our internal references */
static void
seahorse_key_source_dispose (GObject *gobject)
{
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_key_source_finalize (GObject *gobject)
{
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/**
 * seahorse_key_source_load
 * @sksrc: A #SeahorseKeySource object
 * @key: The key to refresh or SEAHORSE_KEY_SOURCE_ALL, SEAHORSE_KEY_SOURCE_NEW 
 * 
 * Refreshes the #SeahorseKeySource's internal key listing. 
 * 
 * Returns the asynchronous key refresh operation. 
 **/   
SeahorseOperation*
seahorse_key_source_load (SeahorseKeySource *sksrc, SeahorseKeySourceLoad load,
                          GQuark keyid, const gchar *match)
{
    SeahorseKeySourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);
    g_return_val_if_fail (klass->load != NULL, NULL);
    
    return (*klass->load) (sksrc, load, keyid, match);
}

/**
 * seahorse_key_source_load_sync
 * @sksrc: A #SeahorseKeySource object
 * @key: The key to refresh or SEAHORSE_KEY_SOURCE_ALL, SEAHORSE_KEY_SOURCE_NEW 
 * 
 * Refreshes the #SeahorseKeySource's internal key listing, and waits for it to complete.
 **/   
void
seahorse_key_source_load_sync (SeahorseKeySource *sksrc, SeahorseKeySourceLoad load,
                               GQuark keyid, const gchar *match)
{
    SeahorseOperation *op = seahorse_key_source_load (sksrc, load, keyid, match);
    g_return_if_fail (op != NULL);
    seahorse_operation_wait (op);
    g_object_unref (op);
}

/**
 * seahorse_key_source_load_sync
 * @sksrc: A #SeahorseKeySource object
 * @key: The key to refresh or SEAHORSE_KEY_SOURCE_ALL, SEAHORSE_KEY_SOURCE_NEW 
 * 
 * Refreshes the #SeahorseKeySource's internal key listing. Completes in the background.
 **/   
void
seahorse_key_source_load_async (SeahorseKeySource *sksrc, SeahorseKeySourceLoad load,
                                GQuark keyid, const gchar *match)
{
    SeahorseOperation *op = seahorse_key_source_load (sksrc, load, keyid, match);
    g_return_if_fail (op != NULL);
    g_object_unref (op);
}

/**
 * seahorse_key_source_stop
 * @sksrc: A #SeahorseKeySource object
 * 
 * Stops all load operations on the #SeahorseKeySource.
 **/
void
seahorse_key_source_stop (SeahorseKeySource *sksrc)
{
    SeahorseKeySourceClass *klass;
    
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);
    g_return_if_fail (klass->stop != NULL);
    
    (*klass->stop) (sksrc);
}

/**
 * seahorse_key_source_get_state
 * @sksrc: A #SeahorseKeySource object
 * 
 * Gets the internal status of the key source object.
 * 
 * Returns: A combination of flags from seahorse-key-source.h
 **/
guint
seahorse_key_source_get_state (SeahorseKeySource *sksrc)
{
    SeahorseKeySourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), 0);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);   
    g_return_val_if_fail (klass->get_state != NULL, 0);
    
    return (*klass->get_state) (sksrc);
}

SeahorseOperation* 
seahorse_key_source_import (SeahorseKeySource *sksrc, gpgme_data_t data)
{
    SeahorseKeySourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);   
    g_return_val_if_fail (klass->import != NULL, NULL);
    
    return (*klass->import) (sksrc, data);  
}

gboolean            
seahorse_key_source_import_sync (SeahorseKeySource *sksrc, gpgme_data_t data,
                                 GError **err)
{
    SeahorseOperation *op;
    gboolean ret;

    op = seahorse_key_source_import (sksrc, data);
    g_return_val_if_fail (op != NULL, FALSE);
    
    seahorse_operation_wait (op);
    ret = seahorse_operation_is_successful (op);
    if (!ret)
        seahorse_operation_steal_error (op, err);
    
    g_object_unref (op);
    return ret;    
}

SeahorseOperation*
seahorse_key_source_export_keys (GList *keys, gpgme_data_t data)
{
    SeahorseOperation *op = NULL;
    SeahorseMultiOperation *mop = NULL;
    SeahorseKeySource *sksrc;
    SeahorseKey *skey;
    gboolean allocated = FALSE;
    GList *next;
    
    if (!data) {
        data = gpgmex_data_new ();
        allocated = TRUE;
    }
    
    /* Sort by key source */
    keys = g_list_copy (keys);
    keys = seahorse_util_keylist_sort (keys);
    
    while (keys) {
     
        /* Break off one set (same keysource) */
        next = seahorse_util_keylist_splice (keys);

        g_assert (SEAHORSE_IS_KEY (keys->data));
        skey = SEAHORSE_KEY (keys->data);

        /* Export from this key source */        
        sksrc = seahorse_key_get_source (skey);
        g_return_val_if_fail (sksrc != NULL, FALSE);
        
        if (op != NULL) {
            if (mop == NULL)
                mop = seahorse_multi_operation_new ();
            seahorse_multi_operation_take (mop, op);
        }
        
        /* We pass our own data object, to which data is appended */
        op = seahorse_key_source_export (sksrc, keys, FALSE, data);
        g_return_val_if_fail (op != NULL, FALSE);

        g_list_free (keys);
        keys = next;
    }
    
    if (mop) {
        op = SEAHORSE_OPERATION (mop);
        
        /* 
         * Setup the result data properly, as we would if it was a 
         * single export operation.
         */
        seahorse_operation_mark_result (op, data, 
                             allocated ? (GDestroyNotify)gpgmex_data_release : NULL);
    }
    
    if (!op) 
        op = seahorse_operation_new_complete (NULL);
    
    return op;
}

SeahorseOperation* 
seahorse_key_source_export (SeahorseKeySource *sksrc, GList *keys, 
                            gboolean complete, gpgme_data_t data)
{
    SeahorseKeySourceClass *klass;
    SeahorseOperation *op;
    GSList *keyids = NULL;
    GList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);   
    if (klass->export) 
        return (*klass->export) (sksrc, keys, complete, data);    

    /* Either export or export_raw must be implemented */
    g_return_val_if_fail (klass->export_raw != NULL, NULL);
    
    for (l = keys; l; l = g_list_next (l)) 
        keyids = g_slist_prepend (keyids, GUINT_TO_POINTER (seahorse_key_get_keyid (l->data)));
    
    keyids = g_slist_reverse (keyids);
    op = (*klass->export_raw) (sksrc, keyids, data);
    g_slist_free (keyids);
    return op;

}

SeahorseOperation* 
seahorse_key_source_export_raw (SeahorseKeySource *sksrc, GSList *keyids, 
                                gpgme_data_t data)
{
    SeahorseKeySourceClass *klass;
    SeahorseOperation *op;
    SeahorseKey *skey;
    GList *keys = NULL;
    GSList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);   
    
    /* Either export or export_raw must be implemented */
    if (klass->export_raw)
        return (*klass->export_raw)(sksrc, keyids, data);
    
    g_return_val_if_fail (klass->export != NULL, NULL);
        
    for (l = keyids; l; l = g_slist_next (l)) {
        skey = seahorse_context_get_key (SCTX_APP (), sksrc, GPOINTER_TO_UINT (l->data));
        
        /* TODO: A proper error message here 'not found' */
        if (skey)
            keys = g_list_prepend (keys, skey);
    }
    
    keys = g_list_reverse (keys);
    op = (*klass->export) (sksrc, keys, FALSE, data);
    g_list_free (keys);
    return op;
}

gboolean            
seahorse_key_source_remove (SeahorseKeySource *sksrc, SeahorseKey *skey,
                            guint name, GError **error)
{
    SeahorseKeySourceClass *klass;
    
    g_assert (!error || !*error);
    g_return_val_if_fail (seahorse_key_get_source (skey) == sksrc, FALSE);
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), FALSE);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);
    g_return_val_if_fail (klass->remove != NULL, FALSE);
    
    return (*klass->remove) (sksrc, skey, name, error);    
}
                                               
GQuark              
seahorse_key_source_get_ktype (SeahorseKeySource *sksrc)
{
    GQuark ktype;
    g_object_get (sksrc, "key-type", &ktype, NULL);
    return ktype;
}

SeahorseKeyLoc   
seahorse_key_source_get_location (SeahorseKeySource *sksrc)
{
    SeahorseKeyLoc loc;
    g_object_get (sksrc, "location", &loc, NULL);
    return loc;
}

/* -----------------------------------------------------------------------------
 * CANONICAL KEYIDS 
 */

GQuark
seahorse_key_source_cannonical_keyid (GQuark ktype, const gchar *keyid)
{
    g_return_val_if_fail (keyid != NULL, 0);
    
    if (ktype == SKEY_PGP)
        return seahorse_pgp_key_get_cannonical_id (keyid);
    
#ifdef WITH_SSH
    else if (ktype == SKEY_SSH)
        return seahorse_ssh_key_get_cannonical_id (keyid);
#endif
        
    else 
        g_return_val_if_reached (0);
    
    return 0;
}
