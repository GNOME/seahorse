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

#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>

#include "seahorse-multi-source.h"
#include "seahorse-util.h"
#include "seahorse-key.h"
    
/* GObject handlers */
static void seahorse_multi_source_class_init (SeahorseMultiSourceClass *klass);
static void seahorse_multi_source_init       (SeahorseMultiSource *psrc);
static void seahorse_multi_source_dispose    (GObject *gobject);
static void seahorse_multi_source_finalize   (GObject *gobject);

/* SeahorseKeySource methods */
static void         seahorse_multi_source_refresh     (SeahorseKeySource *src,
                                                       gboolean all);
static void         seahorse_multi_source_stop        (SeahorseKeySource *src);
static guint        seahorse_multi_source_get_state   (SeahorseKeySource *src);
SeahorseKey*        seahorse_multi_source_get_key     (SeahorseKeySource *source,
                                                       const gchar *fpr,
                                                       SeahorseKeyInfo info);
static GList*       seahorse_multi_source_get_keys    (SeahorseKeySource *src,
                                                       gboolean secret_only);
static guint        seahorse_multi_source_get_count   (SeahorseKeySource *src,
                                                       gboolean secret_only);
static gpgme_ctx_t  seahorse_multi_source_new_context (SeahorseKeySource *src);

static GObjectClass *parent_class = NULL;

/* Forward declaration */
static void         release_key_source                 (SeahorseMultiSource *msrc, 
                                                        SeahorseKeySource *sksrc, 
                                                        gboolean quiet);

GType
seahorse_multi_source_get_type (void)
{
    static GType multi_source_type = 0;
 
    if (!multi_source_type) {
        
        static const GTypeInfo multi_source_info = {
            sizeof (SeahorseMultiSourceClass), NULL, NULL,
            (GClassInitFunc) seahorse_multi_source_class_init, NULL, NULL,
            sizeof (SeahorseMultiSource), 0, (GInstanceInitFunc) seahorse_multi_source_init
        };
        
        multi_source_type = g_type_register_static (SEAHORSE_TYPE_KEY_SOURCE, 
                                "SeahorseMultiSource", &multi_source_info, 0);
    }
  
    return multi_source_type;
}

/* Initialize the basic class stuff */
static void
seahorse_multi_source_class_init (SeahorseMultiSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseKeySourceClass *key_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    key_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
    
    key_class->refresh = seahorse_multi_source_refresh;
    key_class->stop = seahorse_multi_source_stop;
    key_class->get_count = seahorse_multi_source_get_count;
    key_class->get_key = seahorse_multi_source_get_key;
    key_class->get_keys = seahorse_multi_source_get_keys;
    key_class->get_state = seahorse_multi_source_get_state;
    key_class->new_context = seahorse_multi_source_new_context;    
    
    gobject_class->dispose = seahorse_multi_source_dispose;
    gobject_class->finalize = seahorse_multi_source_finalize;
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_multi_source_init (SeahorseMultiSource *msrc)
{
    /* init private vars */
    msrc->sources = NULL;
}

/* dispose of all our internal references */
static void
seahorse_multi_source_dispose (GObject *gobject)
{
    SeahorseMultiSource *msrc;
    GSList *l;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    msrc = SEAHORSE_MULTI_SOURCE (gobject);
    
    for (l = msrc->sources; l; l = g_slist_next (l)) {
        g_assert (SEAHORSE_IS_KEY_SOURCE (l->data));
        release_key_source (msrc, SEAHORSE_KEY_SOURCE (l->data), TRUE);
    }

    g_slist_free (msrc->sources);    
    msrc->sources = NULL;
    
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
seahorse_multi_source_finalize (GObject *gobject)
{
    SeahorseMultiSource *msrc;
  
    msrc = SEAHORSE_MULTI_SOURCE (gobject);
    g_assert (msrc->sources == NULL);
    
    /* We don't own our key source, and parent class tries to free */
    SEAHORSE_KEY_SOURCE(msrc)->ctx = NULL;
    
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/* --------------------------------------------------------------------------
 * METHODS
 */

static void         
seahorse_multi_source_refresh (SeahorseKeySource *src, gboolean all)
{
    SeahorseMultiSource *msrc;
    GSList *l;  
    
    msrc = SEAHORSE_MULTI_SOURCE (src);   

    /* Go through our sources */
    for (l = msrc->sources; l != NULL; l = g_slist_next (l)) {
        g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data));
        seahorse_key_source_refresh (SEAHORSE_KEY_SOURCE (l->data), all);
    }    
}

static void
seahorse_multi_source_stop (SeahorseKeySource *src)
{
    SeahorseMultiSource *msrc;
    GSList *l;  
    
    msrc = SEAHORSE_MULTI_SOURCE (src);   

    /* Go through our sources */
    for (l = msrc->sources; l != NULL; l = g_slist_next (l)) {
        g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data));
        seahorse_key_source_stop (SEAHORSE_KEY_SOURCE (l->data));
    }    
}

static guint
seahorse_multi_source_get_count (SeahorseKeySource *src,
                                 gboolean secret_only)
{
    SeahorseMultiSource *msrc;
    GSList *l;  
    guint n = 0;
    
    msrc = SEAHORSE_MULTI_SOURCE (src);   

    /* Go through our sources */
    for (l = msrc->sources; l != NULL; l = g_slist_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), 0);
        n += seahorse_key_source_get_count (SEAHORSE_KEY_SOURCE (l->data), secret_only);
    }    
    
    return n;
}

SeahorseKey*        
seahorse_multi_source_get_key (SeahorseKeySource *src, const gchar *fpr, 
                               SeahorseKeyInfo info)
{
    SeahorseMultiSource *msrc;
    SeahorseKey *skey = NULL;
    GSList *l;

    msrc = SEAHORSE_MULTI_SOURCE (src);   

    /* Go through our sources */
    for (l = msrc->sources; l != NULL; l = g_slist_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), NULL);
        skey = seahorse_key_source_get_key (SEAHORSE_KEY_SOURCE (l->data), fpr, info);
        
        if (skey != NULL)
            break;
    }    
    
    return skey;
}

static GList*
seahorse_multi_source_get_keys (SeahorseKeySource *src,
                                gboolean secret_only)  
{
    SeahorseMultiSource *msrc;
    GList *keys = NULL;
    GSList *l;
    
    msrc = SEAHORSE_MULTI_SOURCE (src);   

    /* Go through our sources */
    for (l = msrc->sources; l != NULL; l = g_slist_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), keys);
        keys = g_list_concat (keys, 
            seahorse_key_source_get_keys (SEAHORSE_KEY_SOURCE (l->data), secret_only));
    }
    
    return keys;    
}

static guint
seahorse_multi_source_get_state (SeahorseKeySource *src)
{
    SeahorseMultiSource *msrc;
    guint state = 0;
    GSList *l;

    msrc = SEAHORSE_MULTI_SOURCE (src);   

    /* Go through our sources */
    for (l = msrc->sources; l != NULL; l = g_slist_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), state);
        state |= seahorse_key_source_get_state (SEAHORSE_KEY_SOURCE (l->data));
    }
    
    return state;
}

static gpgme_ctx_t  
seahorse_multi_source_new_context (SeahorseKeySource *src)
{
    SeahorseMultiSource *msrc;

    msrc = SEAHORSE_MULTI_SOURCE (src);   
    g_return_val_if_fail (msrc && msrc->sources, NULL);

    return seahorse_key_source_new_context (SEAHORSE_KEY_SOURCE (msrc->sources->data));
}

/* -------------------------------------------------------------------------- 
 * HELPERS
 */

/* Called when a key is added to a key source we own */
static void 
source_key_added (SeahorseKeySource *sksrc, SeahorseKey *skey, SeahorseMultiSource *msrc)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_SOURCE (msrc));
    seahorse_key_source_added (SEAHORSE_KEY_SOURCE (msrc), skey);
}

/* Called when a key is removed from a key source we own */
static void 
source_key_removed (SeahorseKeySource *sksrc, SeahorseKey *skey, SeahorseMultiSource *msrc)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_SOURCE (msrc));
    seahorse_key_source_removed (SEAHORSE_KEY_SOURCE (msrc), skey);
}

/* Called when a key source we own has a status update */
static void 
source_progress (SeahorseKeySource *sksrc, const gchar *msg, double fract, 
                 SeahorseMultiSource *msrc)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_SOURCE (msrc));
    seahorse_key_source_show_progress (SEAHORSE_KEY_SOURCE (msrc), msg, fract);
}

static void
emit_keys_added (SeahorseMultiSource *msrc, SeahorseKeySource *sksrc)
{
    GList *keys, *l;
    
    keys = seahorse_key_source_get_keys (sksrc, FALSE);
    
    for (l = keys; l; l = g_list_next (l)) {
        g_return_if_fail (SEAHORSE_IS_KEY (l->data));
        seahorse_key_source_added (SEAHORSE_KEY_SOURCE (msrc), SEAHORSE_KEY (l->data));
    }
    
    g_list_free (keys);
}
    
static void
emit_keys_removed (SeahorseMultiSource *msrc, SeahorseKeySource *sksrc)
{
    GList *keys, *l;
    
    keys = seahorse_key_source_get_keys (sksrc, FALSE);
    
    for (l = keys; l; l = g_list_next (l)) {
        g_return_if_fail (SEAHORSE_IS_KEY (l->data));
        seahorse_key_source_removed (SEAHORSE_KEY_SOURCE (msrc), SEAHORSE_KEY (l->data));
    }
    
    g_list_free (keys);
}

void
release_key_source (SeahorseMultiSource *msrc, SeahorseKeySource *sksrc, gboolean quiet)
{
    g_signal_handlers_disconnect_by_func (sksrc, source_key_added, msrc);
    g_signal_handlers_disconnect_by_func (sksrc, source_key_removed, msrc);
    g_signal_handlers_disconnect_by_func (sksrc, source_progress, msrc);

    if (!quiet) 
        emit_keys_removed (msrc, sksrc);
    
    g_object_unref (sksrc);
}

/* -------------------------------------------------------------------------- 
 * FUNCTIONS
 */

/**
 * seahorse_multi_source_new
 * 
 * Creates a new Multi key source
 * 
 * Returns: The key source.
 **/
SeahorseMultiSource*
seahorse_multi_source_new (void)
{
   return g_object_new (SEAHORSE_TYPE_MULTI_SOURCE, NULL);
}   

SeahorseKeySource*
seahorse_multi_source_get_primary (SeahorseMultiSource *msrc)
{
    g_return_val_if_fail (SEAHORSE_IS_MULTI_SOURCE (msrc), NULL);
    if (msrc->sources)
        return SEAHORSE_KEY_SOURCE (msrc->sources->data);
    return NULL;
}

/**
 * seahorse_multi_source_add
 * @msrc: The multi source
 * @sksrc: The key source to add
 * @important: Whether the key source has high priority or not
 * 
 * Adds a key source to this key source. It's assumed we own the added source
 * and therefore we don't ref. 
 **/
void        
seahorse_multi_source_add (SeahorseMultiSource *msrc, SeahorseKeySource *sksrc,
                           gboolean important)
{
    g_return_if_fail (SEAHORSE_IS_MULTI_SOURCE (msrc));
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    g_assert(sksrc->ctx);
    
    /* 
     * Make special note that the first source in the list is more 'special'
     * than the rest. It's the one that we generate new_context on, and the 
     * one that our gpgme ctx is from.
     */

    /* Make sure we don't already have this source */
    g_return_if_fail (g_slist_find (msrc->sources, sksrc) == NULL);
        
    if (important)
        msrc->sources = g_slist_prepend (msrc->sources, sksrc);
    else
        msrc->sources = g_slist_append (msrc->sources, sksrc);
        
    /* Listen to the source */
    g_signal_connect (sksrc, "added", G_CALLBACK (source_key_added), msrc);             
    g_signal_connect (sksrc, "removed", G_CALLBACK (source_key_removed), msrc);             
    g_signal_connect (sksrc, "progress", G_CALLBACK (source_progress), msrc);             
        
    emit_keys_added (msrc, sksrc);

    /* Always use the first key sources gpgme ctx as ours */
    SEAHORSE_KEY_SOURCE (msrc)->ctx = SEAHORSE_KEY_SOURCE (msrc->sources->data)->ctx;
}

/**
 * seahorse_multi_source_remove
 * @msrc: The multi source
 * @sksrc: The keysource to remove
 * 
 * Remove a key source from this multi source.
 **/
void 
seahorse_multi_source_remove (SeahorseMultiSource *msrc, SeahorseKeySource *sksrc)
{
    GSList *link;
    
    g_return_if_fail (SEAHORSE_IS_MULTI_SOURCE (msrc));
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
    
    link = g_slist_find (msrc->sources, sksrc);
    if (link != NULL) {
        msrc->sources = g_slist_delete_link (msrc->sources, link);
        release_key_source (msrc, sksrc, FALSE);
    }    

    /* Always use the first key sources gpgme ctx as ours */
    if (msrc->sources)
        SEAHORSE_KEY_SOURCE (msrc)->ctx = SEAHORSE_KEY_SOURCE (msrc->sources->data)->ctx;
    else
        SEAHORSE_KEY_SOURCE (msrc)->ctx = NULL;
}
