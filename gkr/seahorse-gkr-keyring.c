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

#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

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
	
	name = g_strdup_printf (_("Passwords: %s"), self->pv->keyring_name);
	markup = g_markup_printf_escaped (_("<b>Passwords:</b> %s"), self->pv->keyring_name);
	
	g_object_set (self,
	              "label", name,
	              "markup", markup,
	              "nickname", self->pv->keyring_name,
	              "identifier", "",
	              "description", "",
	              "flags", SEAHORSE_FLAG_DELETABLE,
	              "icon", "folder",
	              "usage", SEAHORSE_USAGE_OTHER,
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

typedef struct {
	SeahorseGkrKeyring *keyring;
	gpointer request;
	GCancellable *cancellable;
	gulong cancelled_sig;
} keyring_load_closure;

static void
keyring_load_free (gpointer data)
{
	keyring_load_closure *closure = data;
	if (closure->cancellable && closure->cancelled_sig)
		g_signal_handler_disconnect (closure->cancellable,
		                             closure->cancelled_sig);
	if (closure->cancellable)
		g_object_unref (closure->cancellable);
	g_clear_object (&closure->keyring);
	g_assert (!closure->request);
	g_free (closure);
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
on_keyring_load_list_item_ids (GnomeKeyringResult result,
                               GList *list,
                               gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	keyring_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseObjectPredicate pred;
	SeahorseGkrItem *git;
	const gchar *keyring_name;
	GError *error = NULL;
	GHashTable *checks;
	guint32 item_id;
	GQuark id;

	closure->request = NULL;

	if (seahorse_gkr_propagate_error (result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	keyring_name = seahorse_gkr_keyring_get_name (closure->keyring);
	g_return_if_fail (keyring_name);

	/* When loading new keys prepare a list of current */
	checks = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
	seahorse_object_predicate_clear (&pred);
	pred.source = SEAHORSE_SOURCE (closure->keyring);
	pred.type = SEAHORSE_TYPE_GKR_ITEM;
	seahorse_context_for_objects_full (SCTX_APP (), &pred, insert_id_hashtable, checks);

	while (list) {
		item_id = GPOINTER_TO_UINT (list->data);
		id = seahorse_gkr_item_get_cannonical (keyring_name, item_id);

		git = SEAHORSE_GKR_ITEM (seahorse_context_get_object (seahorse_context_instance (),
		                                                      SEAHORSE_SOURCE (closure->keyring), id));
		if (!git) {
			git = seahorse_gkr_item_new (SEAHORSE_SOURCE (closure->keyring), keyring_name, item_id);

			/* Add to context */
			seahorse_object_set_parent (SEAHORSE_OBJECT (git), SEAHORSE_OBJECT (closure->keyring));
			seahorse_context_take_object (seahorse_context_instance (), SEAHORSE_OBJECT (git));
		}

		g_hash_table_remove (checks, GUINT_TO_POINTER (id));
		list = g_list_next (list);
	}

	g_hash_table_foreach (checks, (GHFunc)remove_key_from_context,
	                      SEAHORSE_SOURCE (closure->keyring));

	g_hash_table_destroy (checks);

	seahorse_progress_end (closure->cancellable, res);
	g_simple_async_result_complete_in_idle (res);
}

static void
on_keyring_load_get_info (GnomeKeyringResult result,
                          GnomeKeyringInfo *info,
                          gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	keyring_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	const gchar *name;

	closure->request = NULL;

	if (seahorse_gkr_propagate_error (result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);

	} else {
		seahorse_gkr_keyring_set_info (closure->keyring, info);

		/* Locked, no items... */
		if (gnome_keyring_info_get_is_locked (info)) {
			on_keyring_load_list_item_ids (GNOME_KEYRING_RESULT_OK, NULL, res);

		} else {
			name = seahorse_gkr_keyring_get_name (closure->keyring);
			closure->request = gnome_keyring_list_item_ids (name,
			                                                on_keyring_load_list_item_ids,
			                                                g_object_ref (res), g_object_unref);
		}
	}
}

static void
on_keyring_load_cancelled (GCancellable *cancellable,
                           gpointer user_data)
{
	keyring_load_closure *closure = user_data;

	if (closure->request)
		gnome_keyring_cancel_request (closure->request);
}

static void
seahorse_gkr_keyring_load_async (SeahorseSource *source,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (source);
	keyring_load_closure *closure;
	GSimpleAsyncResult *res;
#if TODO
/* cancellable = g_cancellable_new ();
g_cancellable_cancel (cancellable); */
#endif

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_gkr_keyring_load_async);

	closure = g_new0 (keyring_load_closure, 1);
	closure->keyring = g_object_ref (self);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, keyring_load_free);

	closure->request = gnome_keyring_get_info (seahorse_gkr_keyring_get_name (self),
	                                           on_keyring_load_get_info,
	                                           g_object_ref (res), g_object_unref);

	if (cancellable)
		closure->cancelled_sig = g_cancellable_connect (cancellable,
		                                                G_CALLBACK (on_keyring_load_cancelled),
		                                                closure, NULL);
	seahorse_progress_prep_and_begin (cancellable, res, NULL);

	g_object_unref (res);
}

static gboolean
seahorse_gkr_keyring_load_finish (SeahorseSource *source,
                                  GAsyncResult *result,
                                  GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source),
	                      seahorse_gkr_keyring_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
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
	iface->load_async = seahorse_gkr_keyring_load_async;
	iface->load_finish = seahorse_gkr_keyring_load_finish;
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
