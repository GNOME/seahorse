/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005, 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
#include <string.h>
#include <libintl.h>

#include "seahorse-bind.h"
#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-predicate.h"
#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-servers.h"
#include "seahorse-util.h"

#ifdef WITH_PGP
#include "pgp/seahorse-server-source.h"
#endif

/**
 * SECTION:seahorse-context
 * @short_description: This is where all the action in a Seahorse process comes together.
 * @include:libseahorse/seahorse-context.h
 *
 **/

/* The application main context */
SeahorseContext* app_context = NULL;


enum {
    DESTROY,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (SeahorseContext, seahorse_context, G_TYPE_OBJECT);

/* 
 * Two hashtables are used to keep track of the objects:
 *
 *  objects_by_source: This contains a reference to the object and allows us to 
 *    lookup objects by their source. Each object/source combination should be 
 *    unique. Hashkeys are made with hashkey_by_source()
 *  objects_by_type: Each value contains a GList of objects with the same id
 *    (ie: same object from different sources). The objects are 
 *    orderred in by preferred usage. 
 */

struct _SeahorseContextPrivate {
    gboolean in_destruction;                /* In destroy signal */
    GSettings *seahorse_settings;
    GSettings *crypto_pgp_settings;
};

static void seahorse_context_dispose    (GObject *gobject);
static void seahorse_context_finalize   (GObject *gobject);

static void
seahorse_context_constructed (GObject *obj)
{
	SeahorseContext *self = SEAHORSE_CONTEXT (obj);

	g_return_if_fail (app_context == NULL);

	G_OBJECT_CLASS(seahorse_context_parent_class)->constructed (obj);

	app_context = self;

	self->pv->seahorse_settings = g_settings_new ("org.gnome.seahorse");

#ifdef WITH_PGP
	/* This is installed by gnome-keyring */
	self->pv->crypto_pgp_settings = g_settings_new ("org.gnome.crypto.pgp");
#endif
}
/**
* klass: The class to initialise
*
* Inits the #SeahorseContextClass. Adds the signals "added", "removed", "changed"
**/
static void
seahorse_context_class_init (SeahorseContextClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->constructed = seahorse_context_constructed;
    gobject_class->dispose = seahorse_context_dispose;
    gobject_class->finalize = seahorse_context_finalize;

    signals[DESTROY] = g_signal_new ("destroy", SEAHORSE_TYPE_CONTEXT,
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseContextClass, destroy),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/* init context, private vars, set prefs, connect signals */
/**
* sctx: The context to initialise
*
* Initialises the Sahorse Context @sctx
*
**/
static void
seahorse_context_init (SeahorseContext *sctx)
{
    /* init private vars */
    sctx->pv = g_new0 (SeahorseContextPrivate, 1);
}

/**
* gobject: the object to dispose (#SeahorseContext)
*
* release all references
*
**/
static void
seahorse_context_dispose (GObject *gobject)
{
	SeahorseContext *sctx = SEAHORSE_CONTEXT (gobject);

	if (!sctx->pv->in_destruction) {
		sctx->pv->in_destruction = TRUE;
		g_signal_emit (sctx, signals[DESTROY], 0);
		sctx->pv->in_destruction = FALSE;
	}

	G_OBJECT_CLASS (seahorse_context_parent_class)->dispose (gobject);
}


/**
* gobject: The #SeahorseContext to finalize
*
* destroy all objects, free private vars
*
**/
static void
seahorse_context_finalize (GObject *gobject)
{
	SeahorseContext *sctx = SEAHORSE_CONTEXT (gobject);
	app_context = NULL;

#ifdef WITH_KEYSERVER
	g_clear_object (&sctx->pv->crypto_pgp_settings);
#endif
	g_clear_object (&sctx->pv->seahorse_settings);

	g_free (sctx->pv);
	G_OBJECT_CLASS (seahorse_context_parent_class)->finalize (gobject);
}

/**
* seahorse_context_instance:
*
* Returns: the application main context as #SeahorseContext
*/
SeahorseContext*
seahorse_context_instance (void)
{
    g_return_val_if_fail (app_context != NULL, NULL);
    return app_context;
}
   
/**
 * seahorse_context_instance:
 *
 * Gets the main seahorse context or returns a new one if none already exist.
 *
 * Returns: The context instance
 */
void
seahorse_context_create (void)
{
	g_return_if_fail (app_context == NULL);
	g_object_new (SEAHORSE_TYPE_CONTEXT, NULL);
	g_return_if_fail (app_context != NULL);
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
	g_return_if_fail (SEAHORSE_IS_CONTEXT (sctx));
	g_return_if_fail (sctx == app_context);
	g_object_run_dispose (G_OBJECT (sctx));
	g_object_unref (sctx);
}

GSettings *
seahorse_context_settings (SeahorseContext *self)
{
	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);

	return self->pv->seahorse_settings;
}

GSettings *
seahorse_context_pgp_settings (SeahorseContext *self)
{
	if (self == NULL)
		self = seahorse_context_instance ();
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (self), NULL);

	return self->pv->crypto_pgp_settings;
}
