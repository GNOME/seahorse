/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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
 
#include <gnome.h>

#include "seahorse-key-source.h"
#include "seahorse-marshal.h"

enum {
    ADDED,
    REMOVED,
    PROGRESS,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/* GObject handlers */
static void seahorse_key_source_class_init (SeahorseKeySourceClass *klass);
static void seahorse_key_source_init       (SeahorseKeySource *sksrc);
static void seahorse_key_source_dispose    (GObject *gobject);
static void seahorse_key_source_finalize   (GObject *gobject);

/* Handlers */
static void gpgme_progress (gpointer data, const gchar *what, gint type, 
                            gint current, gint total);

GType
seahorse_key_source_get_type (void)
{
    static GType key_source_type = 0;
 
    if (!key_source_type) {
        
        static const GTypeInfo key_source_info = {
            sizeof (SeahorseKeySourceClass), NULL, NULL,
            (GClassInitFunc) seahorse_key_source_class_init, NULL, NULL,
            sizeof (SeahorseKeySource), 0, (GInstanceInitFunc) seahorse_key_source_init
        };
        
        key_source_type = g_type_register_static (G_TYPE_OBJECT, 
                                "SeahorseKeySource", &key_source_info, 0);
    }
  
    return key_source_type;
}

static void
seahorse_key_source_class_init (SeahorseKeySourceClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_key_source_dispose;
    gobject_class->finalize = seahorse_key_source_finalize;

    signals[ADDED] = g_signal_new ("added", SEAHORSE_TYPE_KEY_SOURCE, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseKeySourceClass, added),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
    signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_KEY_SOURCE, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseKeySourceClass, removed),
                NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
    signals[PROGRESS] = g_signal_new ("progress", SEAHORSE_TYPE_KEY_SOURCE, 
                G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseKeySourceClass, progress),
                NULL, NULL, seahorse_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_DOUBLE);
}

/* Initialize the object */
static void
seahorse_key_source_init (SeahorseKeySource *sksrc)
{
    /* Setup some stuff on the context */
    // gpgme_set_progress_cb (sksrc->ctx, gpgme_progress, sksrc);
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
    SeahorseKeySource *sksrc;
  
    sksrc = SEAHORSE_KEY_SOURCE (gobject);
    
    if (sksrc->ctx)
        gpgme_release (sksrc->ctx);
        
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/**
 * seahorse_key_source_added
 * @sksrc: A #SeahorseKeySource object
 * @skey: The key that was added
 * 
 * Called by a #SeahorseKeySource when a key is added
 **/
void        
seahorse_key_source_added (SeahorseKeySource *sksrc, SeahorseKey* key)
{
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    g_return_if_fail (SEAHORSE_IS_KEY (key));
    g_signal_emit (sksrc, signals[ADDED], 0, key);
}
  
/**
 * seahorse_key_source_removed
 * @sksrc: A #SeahorseKeySource object
 * @skey: The key that was removed
 * 
 * Called by a #SeahorseKeySource when a key is removed
 **/
 void
seahorse_key_source_removed (SeahorseKeySource *sksrc, SeahorseKey* key)
{
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    g_return_if_fail (SEAHORSE_IS_KEY (key));
    g_signal_emit (sksrc, signals[REMOVED], 0, key);
}

/**
 * seahorse_key_source_show_progress
 * @sksrc: A #SeahorseKeySource object
 * @op: A mesage to show
 * @pos: Progress bar indicator position (or -1)
 * 
 * Called by a #SeahorseKeySource when a progress or status changes
 **/
void
seahorse_key_source_show_progress (SeahorseKeySource *sksrc, const gchar *op, gdouble fract)
{
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    g_signal_emit (sksrc, signals[PROGRESS], 0, op, fract);
}

/**
 * seahorse_key_source_refresh
 * @sksrc: A #SeahorseKeySource object
 * @all: Whether to refresh existing keys, or only find new, and remove old
 * 
 * Refreshes the #SeahorseKeySource's internal key listing. If new_key_hint
 * is non-zero then it just finds the specified number of new keys.
 **/   
void
seahorse_key_source_refresh (SeahorseKeySource *sksrc, gboolean all)
{
    SeahorseKeySourceClass *klass;
    
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);
    g_return_if_fail (klass->refresh != NULL);
    
    (*klass->refresh) (sksrc, all);    
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
 * seahorse_key_source_get_count
 * @sksrc: A #SeahorseKeySource object
 * @secret_only: Only count secret keys
 * 
 * Gets the number of keys (or secret keys) in the #SeahorseKeySource
 * objects internal listing.
 * 
 * Returns: The number of keys.
 **/
guint       
seahorse_key_source_get_count (SeahorseKeySource *sksrc,
                               gboolean secret_only)
{
    SeahorseKeySourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), 0);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);
    g_return_val_if_fail (klass->get_count != NULL, 0);
    
    return (*klass->get_count) (sksrc, secret_only);
} 

/**
 * seahorse_key_source_get_key
 * @sksrc: A #SeahorseKeySource object
 * @fpr: The fingerprint of the desired key
 * @info: The amount of info to load for key.
 * 
 * Finds the specified key based on the fingerprint. If loaded and enough info
 * has already been loaded then just returns a cached version. If info is 
 * not SKEY_INFO_NONE, and key is not loaded then tries to load it.
 * 
 * Returns: The key or NULL if not found.
 **/
SeahorseKey*
seahorse_key_source_get_key (SeahorseKeySource *sksrc, const gchar *fpr,
                             SeahorseKeyInfo info)
{
    SeahorseKeySourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);
    g_return_val_if_fail (klass->get_key != NULL, NULL);
    
    return (*klass->get_key) (sksrc, fpr, info);
}

/**
 * seahorse_key_source_get_keys
 * @sksrc: A #SeahorseKeySource object
 * @secret_only: Only return secret keys
 * 
 * Get a list of all the keys in the key source.
 * 
 * Returns: An allocated list of keys.
 **/
GList*
seahorse_key_source_get_keys (SeahorseKeySource *sksrc,
                              gboolean secret_only)
{
    SeahorseKeySourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);
    g_return_val_if_fail (klass->get_keys != NULL, NULL);
    
    return (*klass->get_keys) (sksrc, secret_only);
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

/**
 * seahorse_key_source_new_context
 * @sksrc: A #SeahorseKeySource object
 * 
 * Makes a new GPGME context which is compatible with this 
 * key source. Make sure to free it with gpgme_release
 * 
 * Returns: The new GPGME context.
 **/
gpgme_ctx_t 
seahorse_key_source_new_context (SeahorseKeySource *sksrc)
{
    SeahorseKeySourceClass *klass;
    
    g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc), NULL);
    klass = SEAHORSE_KEY_SOURCE_GET_CLASS (sksrc);   
    g_return_val_if_fail (klass->new_context != NULL, 0);
    
    return (*klass->new_context) (sksrc);  
}

/* Calc fraction, call ::show_progress() */
static void
gpgme_progress (gpointer data, const gchar *what, gint type, gint current, gint total)
{
   gdouble fract;

    /* do fract */
    if (total == 0)
        fract = 0;
    else if (current == 100 || total == current)
       fract = -1;
    else
       fract = (gdouble)current / (gdouble)total;
 
    seahorse_key_source_show_progress (SEAHORSE_KEY_SOURCE (data), what, fract);
}

