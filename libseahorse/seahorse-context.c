/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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
#include <eel/eel.h>

#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-libdialogs.h"
#include "seahorse-util.h"
#include "seahorse-pgp-source.h"

#define PROGRESS_UPDATE PGP_SCHEMAS "/progress_update"
//#define MAX_THREADS 5

struct _SeahorseContextPrivate {
	GList *sources;
};

enum {
	ADDED,
    REMOVED,
	PROGRESS,
	LAST_SIGNAL
};

static void	seahorse_context_class_init	(SeahorseContextClass *klass);
static void	seahorse_context_init		(SeahorseContext *sctx);
static void	seahorse_context_dispose	(GObject *gobject);
static void seahorse_context_finalize   (GObject *gobject);

/* Signal Handlers */
static void source_key_added   (SeahorseKeySource *sksrc, SeahorseKey *skey, 
                                SeahorseContext *sctx);
static void source_key_removed (SeahorseKeySource *sksrc, SeahorseKey *skey, 
                                SeahorseContext *sctx);
static void source_progress    (SeahorseKeySource *sksrc, const gchar *msg,
                                double fract, SeahorseContext *sctx);


static GtkObjectClass	*parent_class			= NULL;
static guint		context_signals[LAST_SIGNAL]	= { 0 };

GType
seahorse_context_get_type (void)
{
	static GType context_type = 0;
	
	if (!context_type) {
		static const GTypeInfo context_info =
		{
			sizeof (SeahorseContextClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_context_class_init,
			NULL, NULL,
			sizeof (SeahorseContext),
			0,
			(GInstanceInitFunc) seahorse_context_init
		};
		
		context_type = g_type_register_static (GTK_TYPE_OBJECT, "SeahorseContext", &context_info, 0);
	}
	
	return context_type;
}

static void
seahorse_context_class_init (SeahorseContextClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
    gobject_class->dispose = seahorse_context_dispose;
	gobject_class->finalize = seahorse_context_finalize;
	
	klass->added = NULL;
    klass->removed = NULL;
	klass->progress = NULL;
	
	context_signals[ADDED] = g_signal_new ("added", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseContextClass, added),
		NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
    context_signals[REMOVED] = g_signal_new ("removed", G_OBJECT_CLASS_TYPE (gobject_class),
       G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseContextClass, removed),
        NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
	context_signals[PROGRESS] = g_signal_new ("progress", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseContextClass, progress),
		NULL, NULL, seahorse_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_DOUBLE);
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_context_init (SeahorseContext *sctx)
{
	/* init private vars */
	sctx->priv = g_new0 (SeahorseContextPrivate, 1);
}

/* release all references */
static void
seahorse_context_dispose (GObject *gobject)
{
    SeahorseContext *sctx;
    GList *l;
 
    sctx = SEAHORSE_CONTEXT (gobject);

    /* Go through our sources and try to find it */
    for (l = sctx->priv->sources; l != NULL; l = g_list_next (l)) {
        
        /* Make sure to stop all loads */
        seahorse_key_source_stop (SEAHORSE_KEY_SOURCE (l->data));
        
        /* Stop listening to the source */
        g_signal_handlers_disconnect_by_func (G_OBJECT (l->data), source_key_added, sctx);
        g_signal_handlers_disconnect_by_func (G_OBJECT (l->data), source_key_removed, sctx);
        g_signal_handlers_disconnect_by_func (G_OBJECT (l->data), source_progress, sctx);
        
        g_object_unref (G_OBJECT (l->data));
    }
    
    g_list_free (sctx->priv->sources);
   
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* destroy all keys, free private vars */
static void
seahorse_context_finalize (GObject *gobject)
{
	SeahorseContext *sctx;
	
	sctx = SEAHORSE_CONTEXT (gobject);
	g_free (sctx->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}
   
/**
 * seahorse_context_new:
 *
 * Creates a new #SeahorseContext for managing the key ring and preferences.
 *
 * Returns: The new #SeahorseContext
 **/
SeahorseContext*
seahorse_context_new (void)
{
	return g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
}

/**
 * seahorse_context_destroy:
 * @sctx: #SeahorseContext to destroy
 *
 * Emits the destroy signal for @sctx.
 **/
void
seahorse_context_destroy (SeahorseContext *sctx)
{
	g_return_if_fail (GTK_IS_OBJECT (sctx));
	
	gtk_object_destroy (GTK_OBJECT (sctx));
}

/**
 * seahorse_context_load_keys
 * @sctx: #SeahorseContext object
 * @secret_only: Whether to load only secret keys or not 
 * 
 * Loads the default initial setinitial set of keys. 
 **/
void
seahorse_context_load_keys (SeahorseContext *sctx, gboolean secret_only)
{
    SeahorsePGPSource *pgpsrc;
        
    /* Add the default key sources */
    pgpsrc = seahorse_pgp_source_new ();
    seahorse_context_own_source (sctx, SEAHORSE_KEY_SOURCE (pgpsrc));
    seahorse_pgp_source_load (pgpsrc, FALSE);
}

/**
 * seahorse_context_get_n_keys
 * 
 * Gets the number of loaded keys.
 * 
 * Returns: The number of loaded keys.
 */
guint           
seahorse_context_get_n_keys  (SeahorseContext    *sctx)
{
    GList *l = NULL;  
    guint n = 0;

    /* Go through our sources and try to find it */
    for (l = sctx->priv->sources; l != NULL; l = g_list_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), n);
        n += seahorse_key_source_get_count (SEAHORSE_KEY_SOURCE (l->data), FALSE);
    }
    return n;
}

/**
 * seahorse_context_get_keys:
 * @sctx: #SeahorseContext
 *
 * Gets the complete key ring.
 *
 * Returns: A #GList of #SeahorseKey
 **/
GList*
seahorse_context_get_keys (SeahorseContext *sctx)
{
    GList *l, *keypairs = NULL;

    for (l = sctx->priv->sources; l != NULL; l = g_list_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), keypairs);
        keypairs = g_list_concat (keypairs, 
            seahorse_key_source_get_keys (SEAHORSE_KEY_SOURCE (l->data), FALSE));
    }
    return keypairs;       
}

/**
 * seahorse_context_get_n_key_pairs
 * 
 * Gets the number of loaded key pairs.
 * 
 * Returns: The number of loaded key pairs.
 */
guint           
seahorse_context_get_n_key_pairs  (SeahorseContext    *sctx)
{
    GList *l = NULL;  
    guint n = 0;

    /* Go through our sources and try to find it */
    for (l = sctx->priv->sources; l != NULL; l = g_list_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), n);
        n += seahorse_key_source_get_count (SEAHORSE_KEY_SOURCE (l->data), TRUE);
    }
    return n;       
}

/**
 * seahorse_context_get_key_pairs:
 * @sctx: #SeahorseContext
 *
 * Gets the secret key ring
 *
 * Returns: An allocated #GList of #SeahorseKeyPair objects
 **/
GList*
seahorse_context_get_key_pairs (SeahorseContext *sctx)
{
    GList *l, *keypairs = NULL;

    for (l = sctx->priv->sources; l != NULL; l = g_list_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), keypairs);
        keypairs = g_list_concat (keypairs, 
            seahorse_key_source_get_keys (SEAHORSE_KEY_SOURCE (l->data), TRUE));
    }
    return keypairs;       
}

/**
 * seahorse_context_get_key_fpr
 * @sctx: Current #SeahorseContext
 * @fpr: A fingerprint to lookup
 * 
 * Given a fingerprint load or find the key.
 * 
 * Returns: The key or NULL if not found.
 **/
SeahorseKey*    
seahorse_context_get_key_fpr (SeahorseContext *sctx, const char *fpr)
{
    SeahorseKey *skey;
    GList *l;  

    /* Go through our sources and try to find it */
    for (l = sctx->priv->sources; l != NULL; l = g_list_next (l)) {
        g_return_val_if_fail (SEAHORSE_IS_KEY_SOURCE (l->data), NULL);
        
        skey = seahorse_key_source_get_key (SEAHORSE_KEY_SOURCE (l->data), 
                                            fpr, SKEY_INFO_NORMAL);
        if (skey)
            return skey;
    }
    return NULL;
}

/**
 * seahorse_context_get_default_key
 * @sctx: Current #SeahorseContext
 * 
 * Returns: the secret key that's the default key 
 */
SeahorseKeyPair*
seahorse_context_get_default_key (SeahorseContext *sctx)
{
    SeahorseKey *skey = NULL;
    gchar *id;
    
    id = eel_gconf_get_string (DEFAULT_KEY);
    if (id != NULL && id[0]) 
        skey = seahorse_context_get_key_fpr (sctx, id);
    g_free (id);
    
    if (SEAHORSE_IS_KEY_PAIR (skey))
        return SEAHORSE_KEY_PAIR (skey);
    
    return NULL;
}

/**
 * seahorse_context_own_source
 * @sctx: #SeahorseContext object
 * @sksrc: Source to take ownership of
 * 
 * Context takes ownership of specified source, sinks its events
 * destroys it when necessary etc...
 **/
void 
seahorse_context_own_source (SeahorseContext *sctx, SeahorseKeySource *sksrc)
{
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (SEAHORSE_IS_KEY_SOURCE (sksrc));
 
    /* Make sure we don't already have this source */
    g_return_if_fail (g_list_find (sctx->priv->sources, sksrc) == NULL);
    
    sctx->priv->sources = g_list_append (sctx->priv->sources, sksrc);
    /* Note that the sksrc should already be addrefed */

    gpgme_set_passphrase_cb (sksrc->ctx, (gpgme_passphrase_cb_t)seahorse_passphrase_get, sctx);
    
    /* Listen to the source */
    g_signal_connect (sksrc, "added", G_CALLBACK (source_key_added), sctx);             
    g_signal_connect (sksrc, "removed", G_CALLBACK (source_key_removed), sctx);             
    g_signal_connect (sksrc, "progress", G_CALLBACK (source_progress), sctx);             
}

/**
 * seahorse_context_get_pri_source
 * @sctx: #SeahorseContext object
 * 
 * Gets the primary key source for the context
 * 
 * Returns: The primary key source.
 **/
SeahorseKeySource*   
seahorse_context_get_pri_source (SeahorseContext *sctx)
{
    g_return_val_if_fail (sctx->priv->sources != NULL, NULL);
    return SEAHORSE_KEY_SOURCE (sctx->priv->sources->data);
}

/* Called when a key is added to a key source we own */
static void 
source_key_added (SeahorseKeySource *sksrc, SeahorseKey *skey, SeahorseContext *sctx)
{
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_signal_emit (G_OBJECT (sctx), context_signals[ADDED], 0, skey);  
}

/* Called when a key is removed from a key source we own */
static void 
source_key_removed (SeahorseKeySource *sksrc, SeahorseKey *skey, SeahorseContext *sctx)
{
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_signal_emit (G_OBJECT (sctx), context_signals[REMOVED], 0, skey);    
}

/**
 * seahorse_context_show_progress
 * @sctx: A #SeahorseContext object
 * @msg: A progress message
 * @pos: A position for the progress bar, or -1
 **/
void 
seahorse_context_show_progress (SeahorseContext *sctx, const gchar *msg, double pos)
{
    g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
    g_signal_emit (G_OBJECT (sctx), context_signals[PROGRESS], 0, msg, pos);  
}    

/* Called when a key source we own has a status update */
static void 
source_progress (SeahorseKeySource *sksrc, const gchar *msg, double fract, 
                 SeahorseContext *sctx)
{
    seahorse_context_show_progress (sctx, msg, fract);
}
