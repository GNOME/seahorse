/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "config.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-operation.h"

#include <glib/gi18n.h>

/* -----------------------------------------------------------------------------
 * LIST OPERATION 
 */
 

#define SEAHORSE_TYPE_GKR_LIST_OPERATION            (seahorse_gkr_list_operation_get_type ())
#define SEAHORSE_GKR_LIST_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_LIST_OPERATION, SeahorseGkrListOperation))
#define SEAHORSE_GKR_LIST_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_LIST_OPERATION, SeahorseGkrListOperationClass))
#define SEAHORSE_IS_GKR_LIST_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_LIST_OPERATION))
#define SEAHORSE_IS_GKR_LIST_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_LIST_OPERATION))
#define SEAHORSE_GKR_LIST_OPERATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_LIST_OPERATION, SeahorseGkrListOperationClass))

typedef struct _SeahorseGkrListOperation SeahorseGkrListOperation;
typedef struct _SeahorseGkrListOperationClass SeahorseGkrListOperationClass;

struct _SeahorseGkrListOperation {
	SeahorseOperation parent;
	SeahorseGkrKeyring *keyring;
	gpointer request;
};

struct _SeahorseGkrListOperationClass {
	SeahorseOperationClass parent_class;
};

G_DEFINE_TYPE (SeahorseGkrListOperation, seahorse_gkr_list_operation, SEAHORSE_TYPE_OPERATION);

static void
keyring_operation_failed (SeahorseGkrListOperation *self, GnomeKeyringResult result)
{
	SeahorseOperation *op = SEAHORSE_OPERATION (self);
	GError *err = NULL;
    
	g_assert (result != GNOME_KEYRING_RESULT_OK);
	g_assert (result != GNOME_KEYRING_RESULT_CANCELLED);
    
	if (!seahorse_operation_is_running (op))
		return;
    
	if (self->request)
	    gnome_keyring_cancel_request (self->request);
	self->request = NULL;
    
	seahorse_gkr_operation_parse_error (result, &err);
	g_assert (err != NULL);
    
	seahorse_operation_mark_done (op, FALSE, err);
}

/* Remove the given key from the context */
static void
remove_key_from_context (gpointer kt, SeahorseObject *dummy, SeahorseSource *sksrc)
{
	/* This function gets called as a GHFunc on the self->checks hashtable. */
	GQuark keyid = GPOINTER_TO_UINT (kt);
	SeahorseObject *sobj;
    
	sobj = seahorse_context_get_object (SCTX_APP (), sksrc, keyid);
	if (sobj != NULL)
		seahorse_context_remove_object (SCTX_APP (), sobj);
}

static void
insert_id_hashtable (SeahorseObject *object, gpointer user_data)
{
	g_hash_table_insert ((GHashTable*)user_data, 
	                     GUINT_TO_POINTER (seahorse_object_get_id (object)),
	                     GUINT_TO_POINTER (TRUE));
}

static void 
on_list_item_ids (GnomeKeyringResult result, GList *list, SeahorseGkrListOperation *self)
{
	SeahorseObjectPredicate pred;
	SeahorseGkrItem *git;
	const gchar *keyring_name;
	GHashTable *checks;
	guint32 item_id;
	GQuark id;
	
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return;
    
	self->request = NULL;

	if (result != GNOME_KEYRING_RESULT_OK) {
		keyring_operation_failed (self, result);
		return;
	}
	
	keyring_name = seahorse_gkr_keyring_get_name (self->keyring);
	g_return_if_fail (keyring_name);

	/* When loading new keys prepare a list of current */
	checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
	seahorse_object_predicate_clear (&pred);
	pred.source = SEAHORSE_SOURCE (self->keyring);
	pred.type = SEAHORSE_TYPE_GKR_ITEM;
	seahorse_context_for_objects_full (SCTX_APP (), &pred, insert_id_hashtable, checks);
    
	while (list) {
		item_id = GPOINTER_TO_UINT (list->data);
		id = seahorse_gkr_item_get_cannonical (keyring_name, item_id);
		
		git = SEAHORSE_GKR_ITEM (seahorse_context_get_object (SCTX_APP (), 
		                                                      SEAHORSE_SOURCE (self->keyring), id));
		if (!git) {
			git = seahorse_gkr_item_new (SEAHORSE_SOURCE (self->keyring), keyring_name, item_id);

			/* Add to context */ 
			seahorse_object_set_parent (SEAHORSE_OBJECT (git), SEAHORSE_OBJECT (self->keyring));
			seahorse_context_take_object (SCTX_APP (), SEAHORSE_OBJECT (git));
		}
		
		g_hash_table_remove (checks, GUINT_TO_POINTER (id));
		list = g_list_next (list);
	}
	
	g_hash_table_foreach (checks, (GHFunc)remove_key_from_context, 
	                      SEAHORSE_SOURCE (self->keyring));
	
	g_hash_table_destroy (checks);
        
        seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, NULL);
}

static void
on_keyring_info (GnomeKeyringResult result, GnomeKeyringInfo *info, SeahorseGkrListOperation *self)
{
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return;
    
	self->request = NULL;

	if (result != GNOME_KEYRING_RESULT_OK) {
		keyring_operation_failed (self, result);
		return;
	}

	seahorse_gkr_keyring_set_info (self->keyring, info);
	if (gnome_keyring_info_get_is_locked (info)) {
		/* Locked, no items... */
		on_list_item_ids (GNOME_KEYRING_RESULT_OK, NULL, self);
	} else {
		g_object_ref (self);
		self->request = gnome_keyring_list_item_ids (seahorse_gkr_keyring_get_name (self->keyring), 
		                                            (GnomeKeyringOperationGetListCallback)on_list_item_ids, 
		                                            self, g_object_unref);
	}
}

static SeahorseOperation*
start_gkr_list_operation (SeahorseGkrKeyring *keyring)
{
	SeahorseGkrListOperation *self;

	g_assert (SEAHORSE_IS_GKR_KEYRING (keyring));

	self = g_object_new (SEAHORSE_TYPE_GKR_LIST_OPERATION, NULL);
	self->keyring = keyring;
    
	/* Start listing of ids */
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (self), _("Listing passwords"), -1);
	
	g_object_ref (self);
	self->request = gnome_keyring_get_info (seahorse_gkr_keyring_get_name (keyring), 
	                                        (GnomeKeyringOperationGetKeyringInfoCallback)on_keyring_info, 
	                                        self, g_object_unref);
    
	return SEAHORSE_OPERATION (self);
}

static void 
seahorse_gkr_list_operation_cancel (SeahorseOperation *operation)
{
	SeahorseGkrListOperation *self = SEAHORSE_GKR_LIST_OPERATION (operation);    

	if (self->request)
		gnome_keyring_cancel_request (self->request);
	self->request = NULL;
    
	if (seahorse_operation_is_running (operation))
		seahorse_operation_mark_done (operation, TRUE, NULL);
}

static void 
seahorse_gkr_list_operation_init (SeahorseGkrListOperation *self)
{	
	/* Everything already set to zero */
}

static void 
seahorse_gkr_list_operation_dispose (GObject *gobject)
{
	SeahorseGkrListOperation *self = SEAHORSE_GKR_LIST_OPERATION (gobject);
    
	/* Cancel it if it's still running */
	if (seahorse_operation_is_running (SEAHORSE_OPERATION (self)))
		seahorse_gkr_list_operation_cancel (SEAHORSE_OPERATION (self));
	g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (self)));
    
	/* The above cancel should have stopped this */
	g_assert (self->request == NULL);

	G_OBJECT_CLASS (seahorse_gkr_list_operation_parent_class)->dispose (gobject);
}

static void 
seahorse_gkr_list_operation_finalize (GObject *gobject)
{
	SeahorseGkrListOperation *self = SEAHORSE_GKR_LIST_OPERATION (gobject);
	g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (self)));
    
	/* The above cancel should have stopped this */
	g_assert (self->request == NULL);

	G_OBJECT_CLASS (seahorse_gkr_list_operation_parent_class)->finalize (gobject);
}

static void
seahorse_gkr_list_operation_class_init (SeahorseGkrListOperationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseOperationClass *operation_class = SEAHORSE_OPERATION_CLASS (klass);
	
	seahorse_gkr_list_operation_parent_class = g_type_class_peek_parent (klass);
	gobject_class->dispose = seahorse_gkr_list_operation_dispose;
	gobject_class->finalize = seahorse_gkr_list_operation_finalize;
	operation_class->cancel = seahorse_gkr_list_operation_cancel;
    
}

/* -----------------------------------------------------------------------------------------
 * KEYRING
 */

enum {
	PROP_0,
	PROP_SOURCE_TAG,
	PROP_SOURCE_LOCATION,
	PROP_KEYRING_NAME,
	PROP_KEYRING_INFO,
	PROP_IS_DEFAULT
};

struct _SeahorseGkrKeyringPrivate {
	gchar *keyring_name;
	gboolean is_default;
	
	gpointer req_info;
	GnomeKeyringInfo *keyring_info;
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseGkrKeyring, seahorse_gkr_keyring, SEAHORSE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

GType
boxed_type_keyring_info (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("GnomeKeyringInfo", 
		                                     (GBoxedCopyFunc)gnome_keyring_info_copy,
		                                     (GBoxedFreeFunc)gnome_keyring_info_free);
	return type;
}

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
received_keyring_info (GnomeKeyringResult result, GnomeKeyringInfo *info, gpointer data)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (data);
	self->pv->req_info = NULL;
	
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return;
	
	if (result != GNOME_KEYRING_RESULT_OK) {
		/* TODO: Implement so that we can display an error icon, along with some text */
		g_message ("failed to retrieve info for keyring %s: %s",
		           self->pv->keyring_name, 
		           gnome_keyring_result_to_message (result));
		return;
	}

	seahorse_gkr_keyring_set_info (self, info);
}

static void
load_keyring_info (SeahorseGkrKeyring *self)
{
	/* Already in progress */
	if (!self->pv->req_info) {
		g_object_ref (self);
		self->pv->req_info = gnome_keyring_get_info (self->pv->keyring_name,
		                                             received_keyring_info,
		                                             self, g_object_unref);
	}
}

static gboolean
require_keyring_info (SeahorseGkrKeyring *self)
{
	if (!self->pv->keyring_info)
		load_keyring_info (self);
	return self->pv->keyring_info != NULL;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_gkr_keyring_realize (SeahorseObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	gchar *name, *markup;
	
	name = g_strdup_printf ("Passwords: %s", self->pv->keyring_name);
	markup = g_markup_printf_escaped ("<b>Passwords:</b> %s", self->pv->keyring_name);
	
	g_object_set (self,
	              "label", name,
	              "markup", markup,
	              "nickname", self->pv->keyring_name,
	              "identifier", "",
	              "description", "",
	              "flags", 0,
	              "icon", "folder",
	              NULL);
	
	g_free (name);
	g_free (markup);
	
	SEAHORSE_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->realize (obj);
}

static void
seahorse_gkr_keyring_refresh (SeahorseObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);

	if (self->pv->keyring_info)
		load_keyring_info (self);
	
	SEAHORSE_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->refresh (obj);
}

static SeahorseOperation*
seahorse_gkr_keyring_delete (SeahorseObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	return seahorse_gkr_operation_delete_keyring (self);
}

static SeahorseOperation*
seahorse_gkr_keyring_load (SeahorseSource *src)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (src);
	return start_gkr_list_operation (self);
}

static GObject* 
seahorse_gkr_keyring_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->constructor (type, n_props, props);
	SeahorseGkrKeyring *self = NULL;
	gchar *id;
	
	if (obj) {
		self = SEAHORSE_GKR_KEYRING (obj);
		
		g_return_val_if_fail (self->pv->keyring_name, obj);
		id = g_strdup_printf ("%s:%s", SEAHORSE_GKR_TYPE_STR, self->pv->keyring_name);
		g_object_set (self, 
		              "id", g_quark_from_string (id), 
		              "usage", SEAHORSE_USAGE_NONE, 
		              NULL);
		g_free (id);
	}
	
	return obj;
}

static void
seahorse_gkr_keyring_init (SeahorseGkrKeyring *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_KEYRING, SeahorseGkrKeyringPrivate);
	g_object_set (self, "tag", SEAHORSE_GKR_TYPE, "location", SEAHORSE_LOCATION_LOCAL, NULL);
}

static void
seahorse_gkr_keyring_finalize (GObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	
	if (self->pv->keyring_info)
		gnome_keyring_info_free (self->pv->keyring_info);
	self->pv->keyring_info = NULL;
	g_assert (self->pv->req_info == NULL);
    
	g_free (self->pv->keyring_name);
	self->pv->keyring_name = NULL;
	
	if (self->pv->keyring_info) 
		gnome_keyring_info_free (self->pv->keyring_info);
	self->pv->keyring_info = NULL;
	
	G_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->finalize (obj);
}

static void
seahorse_gkr_keyring_set_property (GObject *obj, guint prop_id, const GValue *value, 
                                   GParamSpec *pspec)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	
	switch (prop_id) {
	case PROP_KEYRING_NAME:
		g_return_if_fail (self->pv->keyring_name == NULL);
		self->pv->keyring_name = g_value_dup_string (value);
		g_object_notify (obj, "keyring-name");
		break;
	case PROP_KEYRING_INFO:
		seahorse_gkr_keyring_set_info (self, g_value_get_boxed (value));
		break;
	case PROP_IS_DEFAULT:
		seahorse_gkr_keyring_set_is_default (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_keyring_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	
	switch (prop_id) {
	case PROP_SOURCE_TAG:
		g_value_set_uint (value, SEAHORSE_GKR_TYPE);
		break;
	case PROP_SOURCE_LOCATION:
		g_value_set_enum (value, SEAHORSE_LOCATION_LOCAL);
		break;
	case PROP_KEYRING_NAME:
		g_value_set_string (value, seahorse_gkr_keyring_get_name (self));
		break;
	case PROP_KEYRING_INFO:
		g_value_set_boxed (value, seahorse_gkr_keyring_get_info (self));
		break;
	case PROP_IS_DEFAULT:
		g_value_set_boolean (value, seahorse_gkr_keyring_get_is_default (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_keyring_class_init (SeahorseGkrKeyringClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseObjectClass *seahorse_class;
	
	seahorse_gkr_keyring_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGkrKeyringPrivate));

	gobject_class->constructor = seahorse_gkr_keyring_constructor;
	gobject_class->finalize = seahorse_gkr_keyring_finalize;
	gobject_class->set_property = seahorse_gkr_keyring_set_property;
	gobject_class->get_property = seahorse_gkr_keyring_get_property;
    
	seahorse_class = SEAHORSE_OBJECT_CLASS (klass);
	seahorse_class->realize = seahorse_gkr_keyring_realize;
	seahorse_class->refresh = seahorse_gkr_keyring_refresh;
	seahorse_class->delete = seahorse_gkr_keyring_delete;
	
	g_object_class_override_property (gobject_class, PROP_SOURCE_TAG, "source-tag");
	g_object_class_override_property (gobject_class, PROP_SOURCE_LOCATION, "source-location");

	g_object_class_install_property (gobject_class, PROP_KEYRING_NAME,
	           g_param_spec_string ("keyring-name", "Gnome Keyring Name", "Name of keyring.", 
	                                "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_KEYRING_INFO,
	           g_param_spec_boxed ("keyring-info", "Gnome Keyring Info", "Info about keyring.", 
	                               boxed_type_keyring_info (), G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_IS_DEFAULT,
	           g_param_spec_boolean ("is-default", "Is default", "Is the default keyring.",
	                                 FALSE, G_PARAM_READWRITE));
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load = seahorse_gkr_keyring_load;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGkrKeyring*
seahorse_gkr_keyring_new (const gchar *keyring_name)
{
	return g_object_new (SEAHORSE_TYPE_GKR_KEYRING, "keyring-name", keyring_name, NULL);
}

const gchar*
seahorse_gkr_keyring_get_name (SeahorseGkrKeyring *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (self), NULL);
	return self->pv->keyring_name;
}

GnomeKeyringInfo*
seahorse_gkr_keyring_get_info (SeahorseGkrKeyring *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (self), NULL);
	require_keyring_info (self);
	return self->pv->keyring_info;
}

void
seahorse_gkr_keyring_set_info (SeahorseGkrKeyring *self, GnomeKeyringInfo *info)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (self));
	
	if (self->pv->keyring_info)
		gnome_keyring_info_free (self->pv->keyring_info);
	if (info)
		self->pv->keyring_info = gnome_keyring_info_copy (info);
	else
		self->pv->keyring_info = NULL;
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_gkr_keyring_realize (SEAHORSE_OBJECT (self));
	g_object_notify (obj, "keyring-info");
	g_object_thaw_notify (obj);
}

gboolean
seahorse_gkr_keyring_get_is_default (SeahorseGkrKeyring *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (self), FALSE);
	return self->pv->is_default;
}

void
seahorse_gkr_keyring_set_is_default (SeahorseGkrKeyring *self, gboolean is_default)
{
	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (self));
	self->pv->is_default = is_default;
	g_object_notify (G_OBJECT (self), "is-default");
}
