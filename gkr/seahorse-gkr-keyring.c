/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "seahorse-predicate.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_DESCRIPTION,
	PROP_KEYRING_NAME,
	PROP_KEYRING_INFO,
	PROP_IS_DEFAULT
};

struct _SeahorseGkrKeyringPrivate {
	GHashTable *items;
	gchar *keyring_name;
	gboolean is_default;

	gpointer req_info;
	GnomeKeyringInfo *keyring_info;
};

static void     seahorse_keyring_source_iface        (SeahorseSourceIface *iface);
static void     seahorse_keyring_collection_iface    (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseGkrKeyring, seahorse_gkr_keyring, SEAHORSE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_keyring_collection_iface);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_keyring_source_iface);
);

static GType
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

void
seahorse_gkr_keyring_realize (SeahorseGkrKeyring *self)
{
	const gchar *name;
	GIcon *icon;

	if (self->pv->keyring_name && g_str_equal (self->pv->keyring_name, "login"))
		name = _("Login keyring");
	else
		name = self->pv->keyring_name;

	icon = g_themed_icon_new ("folder");

	g_object_set (self,
	              "label", name,
	              "nickname", self->pv->keyring_name,
	              "identifier", "",
	              "flags", SEAHORSE_FLAG_DELETABLE,
	              "icon", icon,
	              "usage", SEAHORSE_USAGE_OTHER,
	              NULL);

	g_object_unref (icon);
}

void
seahorse_gkr_keyring_refresh (SeahorseGkrKeyring *self)
{
	if (self->pv->keyring_info)
		load_keyring_info (self);
}

void
seahorse_gkr_keyring_remove_item (SeahorseGkrKeyring *self,
                                  guint32 item_id)
{
	SeahorseGkrItem *item;

	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (self));

	item = g_hash_table_lookup (self->pv->items, GUINT_TO_POINTER (item_id));
	if (item != NULL) {
		g_object_ref (item);
		g_object_set (item, "source", NULL, NULL);
		g_hash_table_remove (self->pv->items, GUINT_TO_POINTER (item_id));
		gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (item));
		g_object_unref (item);
	}
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
remove_key_from_context (gpointer kt,
                         gpointer value,
                         gpointer user_data)
{
	/* This function gets called as a GHFunc on the self->checks hashtable. */
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (user_data);
	seahorse_gkr_keyring_remove_item (self, GPOINTER_TO_UINT (kt));
}

static void
on_keyring_load_list_item_ids (GnomeKeyringResult result,
                               GList *list,
                               gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	keyring_load_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
	SeahorseGkrItem *item;
	const gchar *keyring_name;
	GError *error = NULL;
	GHashTable *checks;
	GHashTableIter iter;
	gpointer key;

	closure->request = NULL;

	if (seahorse_gkr_propagate_error (result, &error)) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete_in_idle (res);
		return;
	}

	keyring_name = seahorse_gkr_keyring_get_name (closure->keyring);
	g_return_if_fail (keyring_name);

	/* When loading new keys prepare a list of current */
	checks = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_hash_table_iter_init (&iter, closure->keyring->pv->items);
	while (g_hash_table_iter_next (&iter, &key, NULL))
		g_hash_table_insert (checks, key, key);

	while (list) {
		item = g_hash_table_lookup (closure->keyring->pv->items, list->data);

		if (item == NULL) {
			item = seahorse_gkr_item_new (closure->keyring, keyring_name,
			                              GPOINTER_TO_UINT (list->data));

			/* Add to context */
			g_hash_table_insert (closure->keyring->pv->items, list->data, item);
			gcr_collection_emit_added (GCR_COLLECTION (closure->keyring), G_OBJECT (item));
		}

		g_hash_table_remove (checks, list->data);
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

void
seahorse_gkr_keyring_load_async (SeahorseGkrKeyring *self,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	keyring_load_closure *closure;
	GSimpleAsyncResult *res;

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

gboolean
seahorse_gkr_keyring_load_finish (SeahorseGkrKeyring *self,
                                  GAsyncResult *result,
                                  GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (self),
	                      seahorse_gkr_keyring_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
seahorse_gkr_keyring_constructed (GObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);

	G_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->constructed (obj);

	g_return_if_fail (self->pv->keyring_name);
	g_object_set (self, "usage", SEAHORSE_USAGE_NONE, NULL);
}

static void
seahorse_gkr_keyring_init (SeahorseGkrKeyring *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_KEYRING, SeahorseGkrKeyringPrivate);
	self->pv->items = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
}

static void
seahorse_gkr_keyring_finalize (GObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);

	g_hash_table_destroy (self->pv->items);

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
	case PROP_DESCRIPTION:
		g_value_set_string (value, seahorse_gkr_keyring_get_description (self));
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

	g_type_class_add_private (klass, sizeof (SeahorseGkrKeyringPrivate));

	gobject_class->constructed = seahorse_gkr_keyring_constructed;
	gobject_class->finalize = seahorse_gkr_keyring_finalize;
	gobject_class->set_property = seahorse_gkr_keyring_set_property;
	gobject_class->get_property = seahorse_gkr_keyring_get_property;

	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");

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
seahorse_keyring_source_iface (SeahorseSourceIface *iface)
{

}

static guint
seahorse_gkr_keyring_get_length (GcrCollection *collection)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (collection);
	return g_hash_table_size (self->pv->items);
}

static GList *
seahorse_gkr_keyring_get_objects (GcrCollection *collection)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (collection);
	return g_hash_table_get_values (self->pv->items);
}

static gboolean
seahorse_gkr_keyring_contains (GcrCollection *collection,
                               GObject *object)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (collection);
	guint32 item_id;

	if (!SEAHORSE_IS_GKR_ITEM (object))
		return FALSE;
	item_id = seahorse_gkr_item_get_item_id (SEAHORSE_GKR_ITEM (object));
	return g_hash_table_lookup (self->pv->items, GUINT_TO_POINTER (item_id)) ? TRUE : FALSE;
}

static void
seahorse_keyring_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = seahorse_gkr_keyring_get_length;
	iface->get_objects = seahorse_gkr_keyring_get_objects;
	iface->contains = seahorse_gkr_keyring_contains;
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

const gchar *
seahorse_gkr_keyring_get_description (SeahorseGkrKeyring *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (self), NULL);
	return _("To do Keyring");
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
	seahorse_gkr_keyring_realize (self);
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
