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
    PROP_KEYRING_NAME,
    PROP_KEY_TYPE,
    PROP_FLAGS,
    PROP_KEY_DESC,
    PROP_LOCATION
};

struct _SeahorseGkrSourcePrivate {
	SeahorseMultiOperation *operations;     /* A list of all current operations */    
	gchar *keyring_name;                    /* The key ring name */
	SeahorseObject *keyring_object;		/* Object which represents the whole keyring */
};

G_DEFINE_TYPE (SeahorseGkrSource, seahorse_gkr_source, SEAHORSE_TYPE_SOURCE);

/* -----------------------------------------------------------------------------
 * LIST OPERATION 
 */
 

#define SEAHORSE_TYPE_LIST_OPERATION            (seahorse_list_operation_get_type ())
#define SEAHORSE_LIST_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperation))
#define SEAHORSE_LIST_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperationClass))
#define SEAHORSE_IS_LIST_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_LIST_OPERATION))
#define SEAHORSE_IS_LIST_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_LIST_OPERATION))
#define SEAHORSE_LIST_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_LIST_OPERATION, SeahorseListOperationClass))

DECLARE_OPERATION (List, list)

    SeahorseGkrSource *gsrc;
    gpointer request;
    GHashTable *checks;

END_DECLARE_OPERATION

IMPLEMENT_OPERATION (List, list)

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
    
	seahorse_gkr_operation_parse_error (result, &err);
	g_assert (err != NULL);
    
	seahorse_operation_mark_done (op, FALSE, err);
}

/* Remove the given key from the context */
static void
remove_key_from_context (gpointer kt, SeahorseObject *dummy, SeahorseSource *sksrc)
{
    /* This function gets called as a GHFunc on the lop->checks hashtable. */
    GQuark keyid = GPOINTER_TO_UINT (kt);
    SeahorseObject *sobj;
    
    sobj = seahorse_context_get_object (SCTX_APP (), sksrc, keyid);
    if (sobj != NULL)
        seahorse_context_remove_object (SCTX_APP (), sobj);
}

static void 
keyring_ids_ready (GnomeKeyringResult result, GList *list, SeahorseListOperation *lop)
{
	SeahorseGkrItem *git;
	gchar *keyring_name;
	guint32 item_id;
	GQuark id;
	
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return;
    
	lop->request = NULL;

	if (result != GNOME_KEYRING_RESULT_OK) {
		keyring_operation_failed (lop, result);
		return;
	}
	
	g_object_get (lop->gsrc, "keyring-name", &keyring_name, NULL);
	g_return_if_fail (keyring_name);
    
	while (list) {
		item_id = GPOINTER_TO_UINT (list->data);
		id = seahorse_gkr_item_get_cannonical (keyring_name, item_id);
		
		git = SEAHORSE_GKR_ITEM (seahorse_context_get_object (SCTX_APP (), 
		                                                      SEAHORSE_SOURCE (lop->gsrc), id));
		if (!git) {
			git = seahorse_gkr_item_new (SEAHORSE_SOURCE (lop->gsrc), keyring_name, item_id);

			/* Add to context */ 
			seahorse_object_set_parent (SEAHORSE_OBJECT (git), lop->gsrc->pv->keyring_object);
			seahorse_context_take_object (SCTX_APP (), SEAHORSE_OBJECT (git));
		}
		
		if (lop->checks)
			g_hash_table_remove (lop->checks, GUINT_TO_POINTER (id));
		
		list = g_list_next (list);
	}
	
	if (lop->checks)
        	g_hash_table_foreach (lop->checks, (GHFunc)remove_key_from_context, 
        	                      SEAHORSE_SOURCE (lop->gsrc));
        
        seahorse_operation_mark_done (SEAHORSE_OPERATION (lop), FALSE, NULL);

}

static void
insert_id_hashtable (SeahorseObject *object, gpointer user_data)
{
	g_hash_table_insert ((GHashTable*)user_data, 
	                     GUINT_TO_POINTER (seahorse_object_get_id (object)),
	                     GUINT_TO_POINTER (TRUE));
}

static SeahorseListOperation*
start_list_operation (SeahorseGkrSource *gsrc)
{
	SeahorseListOperation *lop;
	SeahorseObjectPredicate pred;

	g_assert (SEAHORSE_IS_GKR_SOURCE (gsrc));

	lop = g_object_new (SEAHORSE_TYPE_LIST_OPERATION, NULL);
	lop->gsrc = gsrc;
    
	seahorse_operation_mark_start (SEAHORSE_OPERATION (lop));
  
	/* When loading new keys prepare a list of current */
	lop->checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
	seahorse_object_predicate_clear (&pred);
	pred.source = SEAHORSE_SOURCE (gsrc);
	pred.type = SEAHORSE_TYPE_GKR_ITEM;
	seahorse_context_for_objects_full (SCTX_APP (), &pred, insert_id_hashtable, lop->checks);
    
	/* Start listing of ids */
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (lop), _("Listing passwords"), -1);
	g_object_ref (lop);
	lop->request = gnome_keyring_list_item_ids (gsrc->pv->keyring_name, 
	                                            (GnomeKeyringOperationGetListCallback)keyring_ids_ready, 
	                                            lop, g_object_unref);
    
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

    G_OBJECT_CLASS (list_operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_list_operation_finalize (GObject *gobject)
{
    SeahorseListOperation *lop = SEAHORSE_LIST_OPERATION (gobject);
    g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (lop)));
    
    if (lop->checks)
        g_hash_table_destroy (lop->checks);
    lop->checks = NULL;
    
    /* The above cancel should have stopped this */
    g_assert (lop->request == NULL);

    G_OBJECT_CLASS (list_operation_parent_class)->finalize (gobject);
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
 * REMOVE OPERATION 
 */

#define SEAHORSE_TYPE_REMOVE_OPERATION            (seahorse_remove_operation_get_type ())
#define SEAHORSE_REMOVE_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_REMOVE_OPERATION, SeahorseRemoveOperation))
#define SEAHORSE_REMOVE_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_REMOVE_OPERATION, SeahorseRemoveOperationClass))
#define SEAHORSE_IS_REMOVE_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_REMOVE_OPERATION))
#define SEAHORSE_IS_REMOVE_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_REMOVE_OPERATION))
#define SEAHORSE_REMOVE_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_REMOVE_OPERATION, SeahorseRemoveOperationClass))

DECLARE_OPERATION (Remove, remove)

	SeahorseGkrSource *gsrc;
	SeahorseGkrItem *item;
	gpointer request;

END_DECLARE_OPERATION

IMPLEMENT_OPERATION (Remove, remove)

static void 
remove_item_ready (GnomeKeyringResult result, SeahorseRemoveOperation *rop)
{
	GError *err = NULL;
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return;
    
	rop->request = NULL;

	if (result != GNOME_KEYRING_RESULT_OK) {
		if (seahorse_operation_is_running (SEAHORSE_OPERATION (rop))) {
			seahorse_gkr_operation_parse_error (result, &err);
			g_assert (err != NULL);
			seahorse_operation_mark_done (SEAHORSE_OPERATION (rop), FALSE, err);
		}
		return;
	}
    
	seahorse_context_remove_object (SCTX_APP (), SEAHORSE_OBJECT (rop->item));
	seahorse_operation_mark_done (SEAHORSE_OPERATION (rop), FALSE, NULL);
}

static SeahorseRemoveOperation*
start_remove_operation (SeahorseGkrSource *gsrc, SeahorseGkrItem *git)
{
	SeahorseRemoveOperation *rop;

	g_assert (SEAHORSE_IS_GKR_SOURCE (gsrc));
	g_assert (SEAHORSE_IS_GKR_ITEM (git));
	
	rop = g_object_new (SEAHORSE_TYPE_REMOVE_OPERATION, NULL);
	rop->gsrc = gsrc;
	g_object_ref (gsrc);
	rop->item = git;
	g_object_ref (git);
	
	seahorse_operation_mark_start (SEAHORSE_OPERATION (rop));
    
	/* Start listing of ids */
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (rop), _("Removing item"), -1);
	g_object_ref (rop);
	rop->request = gnome_keyring_item_delete (seahorse_gkr_item_get_keyring_name (git), 
	                                          seahorse_gkr_item_get_item_id (git), 
	                                          (GnomeKeyringOperationDoneCallback)remove_item_ready, 
	                                          rop, g_object_unref);
    
	return rop;
}

static void 
seahorse_remove_operation_init (SeahorseRemoveOperation *rop)
{
	/* Everything already set to zero */
}

static void 
seahorse_remove_operation_dispose (GObject *gobject)
{
	SeahorseRemoveOperation *rop = SEAHORSE_REMOVE_OPERATION (gobject);
    
	/* Cancel it if it's still running */
	if (seahorse_operation_is_running (SEAHORSE_OPERATION (rop)))
		seahorse_remove_operation_cancel (SEAHORSE_OPERATION (rop));
	g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (rop)));
    
	/* The above cancel should have stopped this */
	g_assert (rop->request == NULL);
	
	if (rop->gsrc)
		g_object_unref (rop->gsrc);
	rop->gsrc = NULL;
	
	if (rop->item)
		g_object_unref (rop->item);
	rop->item = NULL;

	G_OBJECT_CLASS (remove_operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_remove_operation_finalize (GObject *gobject)
{
	SeahorseRemoveOperation *rop = SEAHORSE_REMOVE_OPERATION (gobject);
	g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (rop)));
    
	g_assert (rop->gsrc == NULL);
	g_assert (rop->item == NULL);
    
	/* The above cancel should have stopped this */
	g_assert (rop->request == NULL);

	G_OBJECT_CLASS (remove_operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_remove_operation_cancel (SeahorseOperation *operation)
{
	SeahorseRemoveOperation *rop = SEAHORSE_REMOVE_OPERATION (operation);    

	if (rop->request)
		gnome_keyring_cancel_request (rop->request);
	rop->request = NULL;
    
	if (seahorse_operation_is_running (operation))
		seahorse_operation_mark_done (operation, TRUE, NULL);
}

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_gkr_source_init (SeahorseGkrSource *gsrc)
{
    /* init private vars */
    gsrc->pv = g_new0 (SeahorseGkrSourcePrivate, 1);
    gsrc->pv->operations = seahorse_multi_operation_new ();
}

static GObject*  
seahorse_gkr_source_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
	GObject* obj = G_OBJECT_CLASS (seahorse_gkr_source_parent_class)->constructor (type, n_props, props);
	SeahorseGkrSource *self = NULL;
	
	if (obj) {
		self = SEAHORSE_GKR_SOURCE (obj);
		
		/* Add the initial keyring folder style object */
		self->pv->keyring_object = SEAHORSE_OBJECT (seahorse_gkr_keyring_new (self->pv->keyring_name));
		seahorse_context_add_object (SCTX_APP (), self->pv->keyring_object);
	}
	
	return obj;
}

static void 
seahorse_gkr_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                       GParamSpec *pspec)
{
    SeahorseGkrSource *gsrc = SEAHORSE_GKR_SOURCE (object);

    switch (prop_id) {
    case PROP_KEYRING_NAME:
        g_value_set_string (value, gsrc->pv->keyring_name);
        break;
    case PROP_KEY_TYPE:
        g_value_set_uint (value, SEAHORSE_GKR);
        break;
    case PROP_FLAGS:
	g_value_set_uint (value, 0);
	break;
    case PROP_KEY_DESC:
        g_value_set_string (value, _("Password"));
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, SEAHORSE_LOCATION_LOCAL);
        break;
    }
}

static void 
seahorse_gkr_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                       GParamSpec *pspec)
{
    SeahorseGkrSource *gsrc = SEAHORSE_GKR_SOURCE (object);

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
seahorse_gkr_source_load (SeahorseSource *src, GQuark keyid)
{
    SeahorseGkrSource *gsrc;
    SeahorseListOperation *lop;
    
    g_assert (SEAHORSE_IS_SOURCE (src));
    gsrc = SEAHORSE_GKR_SOURCE (src);
    
    /* TODO: Loading a specific key? */
    
    lop = start_list_operation (gsrc);
    seahorse_multi_operation_take (gsrc->pv->operations, SEAHORSE_OPERATION (lop));
    
    g_object_ref (lop);
    return SEAHORSE_OPERATION (lop);
}

static SeahorseOperation* 
seahorse_gkr_source_import (SeahorseSource *sksrc, GInputStream *input)
{
	GError *err = NULL;

	g_return_val_if_fail (SEAHORSE_IS_GKR_SOURCE (sksrc), NULL);
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);

	/* TODO: Implement properly */
	g_set_error (&err, 0, 0, "gnome-keyring import support not implemented");
	return seahorse_operation_new_complete (err);   
}

static SeahorseOperation* 
seahorse_gkr_source_export (SeahorseSource *sksrc, GList *keys, GOutputStream *output)
{
	GError *err = NULL;
    
	g_return_val_if_fail (SEAHORSE_IS_GKR_SOURCE (sksrc), NULL);
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
    
	/* TODO: Implement properly */
	g_set_error (&err, 0, 0, "gnome-keyring export support not implemented");
	return seahorse_operation_new_complete (err);    
}

static SeahorseOperation*            
seahorse_gkr_source_remove (SeahorseSource *sksrc, SeahorseObject *sobj)
{
    SeahorseGkrItem *git;
    SeahorseGkrSource *gsrc;
    
    g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (sobj), FALSE);
    git = SEAHORSE_GKR_ITEM (sobj);

    g_return_val_if_fail (SEAHORSE_IS_GKR_SOURCE (sksrc), FALSE);
    gsrc = SEAHORSE_GKR_SOURCE (sksrc);

    return SEAHORSE_OPERATION (start_remove_operation (gsrc, git));
}

static void
seahorse_gkr_source_dispose (GObject *gobject)
{
    SeahorseGkrSource *gsrc;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    gsrc = SEAHORSE_GKR_SOURCE (gobject);
    g_assert (gsrc->pv);
    
    /* Clear out all operations */
    if (gsrc->pv->operations) {
        if(seahorse_operation_is_running (SEAHORSE_OPERATION (gsrc->pv->operations)))
            seahorse_operation_cancel (SEAHORSE_OPERATION (gsrc->pv->operations));
        g_object_unref (gsrc->pv->operations);
        gsrc->pv->operations = NULL;
    }
    
    /* The keyring object */
    if (gsrc->pv->keyring_object)
	    g_object_unref (gsrc->pv->keyring_object);
    gsrc->pv->keyring_object = NULL;

    G_OBJECT_CLASS (seahorse_gkr_source_parent_class)->dispose (gobject);
}

static void
seahorse_gkr_source_finalize (GObject *gobject)
{
    SeahorseGkrSource *gsrc;
  
    gsrc = SEAHORSE_GKR_SOURCE (gobject);
    g_assert (gsrc->pv);
    
    g_assert (gsrc->pv->operations == NULL);
    
    g_free (gsrc->pv->keyring_name);
    g_free (gsrc->pv);
    
    G_OBJECT_CLASS (seahorse_gkr_source_parent_class)->finalize (gobject);
}

static void
seahorse_gkr_source_class_init (SeahorseGkrSourceClass *klass)
{
    GObjectClass *gobject_class;
    SeahorseSourceClass *key_class;
    
    seahorse_gkr_source_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->constructor = seahorse_gkr_source_constructor;
    gobject_class->dispose = seahorse_gkr_source_dispose;
    gobject_class->finalize = seahorse_gkr_source_finalize;
    gobject_class->set_property = seahorse_gkr_source_set_property;
    gobject_class->get_property = seahorse_gkr_source_get_property;
    
    key_class = SEAHORSE_SOURCE_CLASS (klass);    
    key_class->load = seahorse_gkr_source_load;
    key_class->import = seahorse_gkr_source_import;
    key_class->export = seahorse_gkr_source_export;
    key_class->remove = seahorse_gkr_source_remove;

    g_object_class_install_property (gobject_class, PROP_KEYRING_NAME,
        g_param_spec_string ("keyring-name", "Keyring Name", "GNOME Keyring name",
                             NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
    g_object_class_install_property (gobject_class, PROP_KEY_TYPE,
        g_param_spec_uint ("key-type", "Key Type", "Key type that originates from this key source.", 
                           0, G_MAXUINT, SEAHORSE_TAG_INVALID, G_PARAM_READABLE));
    
    g_object_class_install_property (gobject_class, PROP_FLAGS,
        g_param_spec_uint ("flags", "Flags", "Object Source flags.", 
                           0, G_MAXUINT, 0, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_KEY_DESC,
        g_param_spec_string ("key-desc", "Key Desc", "Description for keys that originate here.",
                             NULL, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOCATION,
        g_param_spec_uint ("location", "Key Location", "Where the key is stored. See SeahorseLocation", 
                           0, G_MAXUINT, SEAHORSE_LOCATION_INVALID, G_PARAM_READABLE));    
    
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_GKR_SOURCE, "source", "local", SEAHORSE_GKR_STR, NULL);
}

/* -------------------------------------------------------------------------- 
 * PUBLIC
 */

SeahorseGkrSource*
seahorse_gkr_source_new (const gchar *keyring_name)
{
   return g_object_new (SEAHORSE_TYPE_GKR_SOURCE, "keyring-name", keyring_name, NULL);
}   

const gchar*
seahorse_gkr_source_get_keyring_name (SeahorseGkrSource *gsrc)
{
    return gsrc->pv->keyring_name;
}
