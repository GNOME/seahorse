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

#include "config.h"

#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-libdialogs.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"
#include "seahorse-multi-source.h"
#include "seahorse-pgp-source.h"
#include "seahorse-dns-sd.h"

struct _SeahorseContextPrivate {
    SeahorseKeySource *source;
    SeahorseServiceDiscovery *discovery;
};

static void	seahorse_context_class_init	(SeahorseContextClass *klass);
static void	seahorse_context_init		(SeahorseContext *sctx);
static void	seahorse_context_dispose	(GObject *gobject);
static void seahorse_context_finalize   (GObject *gobject);

static GtkObjectClass	*parent_class			= NULL;

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
}

/* init context, private vars, set prefs, connect signals */
static void
seahorse_context_init (SeahorseContext *sctx)
{
	/* init private vars */
	sctx->priv = g_new0 (SeahorseContextPrivate, 1);

    /* Our multi source */
    sctx->priv->source = SEAHORSE_KEY_SOURCE (seahorse_multi_source_new ());

    sctx->priv->discovery = seahorse_service_discovery_new ();
    
    /* The context is explicitly destroyed */
    g_object_ref (sctx);
}

/* release all references */
static void
seahorse_context_dispose (GObject *gobject)
{
    SeahorseContext *sctx;
 
    sctx = SEAHORSE_CONTEXT (gobject);

    if (sctx->priv->source) {
        
        /* Make sure to stop all loads */
        seahorse_key_source_stop (sctx->priv->source);
        
        g_object_unref (sctx->priv->source);
        sctx->priv->source = NULL;
    }

    if (sctx->priv->discovery) 
        g_object_unref (sctx->priv->discovery);
    sctx->priv->discovery = NULL;

    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* destroy all keys, free private vars */
static void
seahorse_context_finalize (GObject *gobject)
{
	SeahorseContext *sctx;
	
	sctx = SEAHORSE_CONTEXT (gobject);
    g_assert (sctx->priv->source == NULL);
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
    
    id = seahorse_gconf_get_string (DEFAULT_KEY);
    if (id != NULL && id[0]) 
        skey = seahorse_key_source_get_key (sctx->priv->source, id);
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

    g_assert (sctx->priv->source != NULL); 
    seahorse_multi_source_add (SEAHORSE_MULTI_SOURCE (sctx->priv->source), sksrc, FALSE);
    gpgme_set_passphrase_cb (sksrc->ctx, (gpgme_passphrase_cb_t)seahorse_passphrase_get, sctx);
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
seahorse_context_get_key_source (SeahorseContext *sctx)
{
    g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
    g_return_val_if_fail (sctx->priv->source != NULL, NULL);
    return sctx->priv->source;
}

/**
 * seahorse_context_get_discovery
 * @sctx: #SeahorseContext object
 * 
 * Gets the Service Discovery object for this context.
 *
 * Return: The Service Discovery object. 
 */
SeahorseServiceDiscovery*
seahorse_context_get_discovery (SeahorseContext *sctx)
{
    g_return_val_if_fail (sctx->priv->discovery != NULL, NULL);
    return sctx->priv->discovery;
}
