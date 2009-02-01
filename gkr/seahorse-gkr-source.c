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
#include <string.h>
#include <libintl.h>

#include <glib/gi18n.h>

#include "seahorse-operation.h"
#include "seahorse-util.h"
#include "seahorse-secure-memory.h"
#include "seahorse-passphrase.h"

#include "seahorse-gkr-item.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-operation.h"
#include "seahorse-gkr-source.h"

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
    PROP_SOURCE_TAG,
    PROP_SOURCE_LOCATION,
    PROP_FLAGS
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseGkrSource, seahorse_gkr_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

/* -----------------------------------------------------------------------------
 * LIST OPERATION 
 */
 

#define SEAHORSE_TYPE_LIST_OPERATION            (seahorse_list_operation_get_type ())
#define SEAHORSE_LIST_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperation))
#define SEAHORSE_LIST_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperationClass))
#define SEAHORSE_IS_LIST_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_LIST_OPERATION))
#define SEAHORSE_IS_LIST_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_LIST_OPERATION))
#define SEAHORSE_LIST_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperationClass))

typedef struct _SeahorseListOperation SeahorseListOperation;
typedef struct _SeahorseListOperationClass SeahorseListOperationClass;

struct _SeahorseListOperation {
	SeahorseOperation parent;
	SeahorseGkrSource *gsrc;
	gpointer request;
};

struct _SeahorseListOperationClass {
	SeahorseOperationClass parent_class;
};

G_DEFINE_TYPE (SeahorseListOperation, seahorse_list_operation, SEAHORSE_TYPE_OPERATION);

static void
remove_each_keyring_from_context (const gchar *keyring_name, SeahorseObject *keyring, 
                                  gpointer unused)
{
	seahorse_context_remove_object (NULL, keyring);
	seahorse_context_remove_source (NULL, SEAHORSE_SOURCE (keyring));
}

static void
insert_each_keyring_in_hashtable (SeahorseObject *object, gpointer user_data)
{
	g_hash_table_insert ((GHashTable*)user_data, 
	                     g_strdup (seahorse_gkr_keyring_get_name (SEAHORSE_GKR_KEYRING (object))),
	                     g_object_ref (object));
}

static void 
on_keyring_names (GnomeKeyringResult result, GList *list, SeahorseListOperation *self)
{
	SeahorseGkrKeyring *keyring;
	SeahorseObjectPredicate pred;
	GError *err = NULL;
	gchar *keyring_name;
	GHashTable *checks;
	GList *l;

	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return;

	self->request = NULL;

	if (!seahorse_operation_is_running (SEAHORSE_OPERATION (self)))
		return;

	if (result != GNOME_KEYRING_RESULT_OK) {
	    
		g_assert (result != GNOME_KEYRING_RESULT_OK);
		g_assert (result != GNOME_KEYRING_RESULT_CANCELLED);
	    
		if (self->request)
			gnome_keyring_cancel_request (self->request);
		self->request = NULL;
	    
		seahorse_gkr_operation_parse_error (result, &err);
		g_assert (err != NULL);
		
		seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, err);
		return;
	}
	
	/* Load up a list of all the current names */
	checks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	seahorse_object_predicate_clear (&pred);
	pred.source = SEAHORSE_SOURCE (self->gsrc);
	pred.type = SEAHORSE_TYPE_GKR_KEYRING;
	seahorse_context_for_objects_full (NULL, &pred, insert_each_keyring_in_hashtable, checks);

	for (l = list; l; l = g_list_next (l)) {
		keyring_name = l->data;
		
		/* Don't show the 'session' keyring */
		if (g_str_equal (keyring_name, "session"))
			continue;
		
		/* Register a new keyring if possible */
		if (!g_hash_table_remove (checks, keyring_name)) {
			keyring = seahorse_gkr_keyring_new (keyring_name);
			g_object_set (keyring, "source", self->gsrc, NULL);
			seahorse_context_add_source (NULL, SEAHORSE_SOURCE (keyring));
			seahorse_context_take_object (NULL, SEAHORSE_OBJECT (keyring));
		}
		
	}
	
	g_hash_table_foreach (checks, (GHFunc)remove_each_keyring_from_context, NULL);
	g_hash_table_destroy (checks);
        
        seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, NULL);
}

static SeahorseOperation*
start_list_operation (SeahorseGkrSource *gsrc)
{
	SeahorseListOperation *self;

	g_assert (SEAHORSE_IS_GKR_SOURCE (gsrc));

	self = g_object_new (SEAHORSE_TYPE_LIST_OPERATION, NULL);
	self->gsrc = g_object_ref (gsrc);
    
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (self), _("Listing password keyrings"), -1);
  
	/* Start listing of ids */
	g_object_ref (self);
	self->request = gnome_keyring_list_keyring_names ((GnomeKeyringOperationGetListCallback)on_keyring_names, 
	                                                  self, g_object_unref);
    
	return SEAHORSE_OPERATION (self);
}

static void 
seahorse_list_operation_cancel (SeahorseOperation *operation)
{
	SeahorseListOperation *self = SEAHORSE_LIST_OPERATION (operation);    

	if (self->request)
		gnome_keyring_cancel_request (self->request);
	self->request = NULL;
    
	if (seahorse_operation_is_running (operation))
		seahorse_operation_mark_done (operation, TRUE, NULL);
}

static void 
seahorse_list_operation_init (SeahorseListOperation *self)
{
	/* Everything already set to zero */
}

static void 
seahorse_list_operation_dispose (GObject *gobject)
{
	SeahorseListOperation *self = SEAHORSE_LIST_OPERATION (gobject);
    
	/* Cancel it if it's still running */
	if (seahorse_operation_is_running (SEAHORSE_OPERATION (self)))
		seahorse_list_operation_cancel (SEAHORSE_OPERATION (self));
	g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (self)));
    
	/* The above cancel should have stopped this */
	g_assert (self->request == NULL);
	
	if (self->gsrc)
		g_object_unref (self->gsrc);
	self->gsrc = NULL;

	G_OBJECT_CLASS (seahorse_list_operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_list_operation_finalize (GObject *gobject)
{
	SeahorseListOperation *self = SEAHORSE_LIST_OPERATION (gobject);
	g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (self)));
    
	/* The above cancel should have stopped this */
	g_assert (self->request == NULL);

	G_OBJECT_CLASS (seahorse_list_operation_parent_class)->finalize (gobject);
}

static void
seahorse_list_operation_class_init (SeahorseListOperationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseOperationClass *operation_class = SEAHORSE_OPERATION_CLASS (klass);
	
	seahorse_list_operation_parent_class = g_type_class_peek_parent (klass);
	gobject_class->dispose = seahorse_list_operation_dispose;
	gobject_class->finalize = seahorse_list_operation_finalize;
	operation_class->cancel = seahorse_list_operation_cancel;   
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_gkr_source_init (SeahorseGkrSource *self)
{
	self->pv = NULL;
}

static void 
seahorse_gkr_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                       GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_SOURCE_TAG:
		g_value_set_uint (value, SEAHORSE_GKR);
		break;
	case PROP_SOURCE_LOCATION:
		g_value_set_enum (value, SEAHORSE_LOCATION_LOCAL);
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, 0);
		break;
	}
}

static void 
seahorse_gkr_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                  GParamSpec *pspec)
{

}

static SeahorseOperation*
seahorse_gkr_source_load (SeahorseSource *src)
{
	SeahorseGkrSource *self = SEAHORSE_GKR_SOURCE (src);
	return start_list_operation (self);
}

static void
seahorse_gkr_source_class_init (SeahorseGkrSourceClass *klass)
{
	GObjectClass *gobject_class;
    
	seahorse_gkr_source_parent_class = g_type_class_peek_parent (klass);

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->set_property = seahorse_gkr_source_set_property;
	gobject_class->get_property = seahorse_gkr_source_get_property;
    
	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_uint ("flags", "Flags", "Object Source flags.", 
	                            0, G_MAXUINT, 0, G_PARAM_READABLE));

	g_object_class_override_property (gobject_class, PROP_SOURCE_TAG, "source-tag");
	g_object_class_override_property (gobject_class, PROP_SOURCE_LOCATION, "source-location");
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_GKR_SOURCE, "source", "local", SEAHORSE_GKR_STR, NULL);
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load = seahorse_gkr_source_load;
}

/* -------------------------------------------------------------------------- 
 * PUBLIC
 */

SeahorseGkrSource*
seahorse_gkr_source_new (void)
{
   return g_object_new (SEAHORSE_TYPE_GKR_SOURCE, NULL);
}   
