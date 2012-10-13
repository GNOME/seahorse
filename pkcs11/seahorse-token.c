/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include <glib/gi18n.h>

#include "seahorse-certificate.h"
#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-helpers.h"
#include "seahorse-private-key.h"
#include "seahorse-token.h"

#include "seahorse-lockable.h"
#include "seahorse-place.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

enum {
	PROP_0,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_ICON,
	PROP_SLOT,
	PROP_FLAGS,
	PROP_URI,
	PROP_ACTIONS,
	PROP_INFO,
	PROP_LOCKABLE,
	PROP_UNLOCKABLE,
	PROP_SESSION
};

struct _SeahorseTokenPrivate {
	GckSlot *slot;
	gchar *uri;
	GckTokenInfo *info;
	GArray *mechanisms;
	GckSession *session;
	GHashTable *object_for_handle;
	GHashTable *objects_for_id;
	GHashTable *id_for_object;
	GHashTable *objects_visible;
};

static void          seahorse_token_place_iface          (SeahorsePlaceIface *iface);

static void          seahorse_token_collection_iface     (GcrCollectionIface *iface);

static void          seahorse_token_lockable_iface       (SeahorseLockableIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseToken, seahorse_token, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_token_collection_iface);
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_PLACE, seahorse_token_place_iface);
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_LOCKABLE, seahorse_token_lockable_iface);
);

static void
update_mechanisms (SeahorseToken *self)
{
	GArray *mechanisms;

	mechanisms = gck_slot_get_mechanisms (self->pv->slot);
	if (self->pv->mechanisms != NULL)
		g_array_unref (self->pv->mechanisms);
	self->pv->mechanisms = mechanisms;
}

static void
update_token_info (SeahorseToken *self)
{
	GckTokenInfo *info;
	GObject *obj;

	info = gck_slot_get_token_info (self->pv->slot);
	if (info != NULL) {
		gck_token_info_free (self->pv->info);
		self->pv->info = info;

		obj = G_OBJECT (self);
		g_object_notify (obj, "info");
		g_object_notify (obj, "lockable");
		g_object_notify (obj, "unlockable");
	}
}

static void
update_id_map (SeahorseToken *self,
               gpointer object,
               const GckAttribute *id)
{
	GPtrArray *objects;
	GckAttribute *pid;
	gboolean remove = FALSE;
	gboolean add = FALSE;

	/* See if this object was here before */
	pid = g_hash_table_lookup (self->pv->id_for_object, object);

	/* Decide what we're going to do */
	if (id == NULL) {
		if (pid != NULL) {
			id = pid;
			remove = TRUE;
		}
	} else {
		if (pid == NULL) {
			add = TRUE;
		} else if (!gck_attribute_equal (id, pid)) {
			remove = TRUE;
			add = TRUE;
		}
	}

	if (add) {
		if (!g_hash_table_lookup_extended (self->pv->objects_for_id, id,
		                                   (gpointer *)&id, (gpointer *)&objects)) {
			objects = g_ptr_array_new ();
			g_hash_table_insert (self->pv->objects_for_id,
			                     gck_attribute_dup (id), objects);
		}
		g_ptr_array_add (objects, object);
		g_hash_table_insert (self->pv->id_for_object, object, (GckAttribute *)id);
	}

	/* Remove this object from the map */
	if (remove) {
		if (!g_hash_table_remove (self->pv->id_for_object, object))
			g_assert_not_reached ();
		objects = g_hash_table_lookup (self->pv->objects_for_id, id);
		g_assert (objects != NULL);
		g_assert (objects->len > 0);
		if (objects->len == 1) {
			if (!g_hash_table_remove (self->pv->objects_for_id, id))
				g_assert_not_reached ();
		} else {
			if (!g_ptr_array_remove (objects, object))
				g_assert_not_reached ();
		}
	}
}

static gpointer
lookup_id_map (SeahorseToken *self,
               GType object_type,
               const GckAttribute *id)
{
	GPtrArray *objects;
	guint i;

	if (id == NULL)
		return NULL;

	objects = g_hash_table_lookup (self->pv->objects_for_id, id);
	if (objects == NULL)
		return NULL;

	for (i = 0; i < objects->len; i++) {
		if (G_TYPE_CHECK_INSTANCE_TYPE (objects->pdata[i], object_type))
			return objects->pdata[i];
	}

	return NULL;
}

static void
update_visibility (SeahorseToken *self,
                   GList *objects,
                   gboolean visible)
{
	gboolean have;
	gpointer object;
	GList *l;

	for (l = objects; l != NULL; l = g_list_next (l)) {
		object = l->data;
		have = g_hash_table_lookup (self->pv->objects_visible, object) != NULL;
		if (!have && visible) {
			g_hash_table_insert (self->pv->objects_visible, object, object);
			gcr_collection_emit_added (GCR_COLLECTION (self), object);
		} else if (have && !visible) {
			if (!g_hash_table_remove (self->pv->objects_visible, object))
				g_assert_not_reached ();
			gcr_collection_emit_removed (GCR_COLLECTION (self), object);
		}
	}
}

static gboolean
make_certificate_key_pair (SeahorseCertificate *certificate,
                           SeahorsePrivateKey *private_key)
{
	if (seahorse_certificate_get_partner (certificate) ||
	    seahorse_private_key_get_partner (private_key))
		return FALSE;

	seahorse_certificate_set_partner (certificate, private_key);
	seahorse_private_key_set_partner (private_key, certificate);
	return TRUE;
}

static gpointer
break_certificate_key_pair (gpointer object)
{
	SeahorseCertificate *certificate = NULL;
	SeahorsePrivateKey *private_key = NULL;
	gpointer pair;

	if (SEAHORSE_IS_CERTIFICATE (object)) {
		certificate = SEAHORSE_CERTIFICATE (object);
		pair = seahorse_certificate_get_partner (certificate);
		seahorse_certificate_set_partner (certificate, NULL);
	} else if (SEAHORSE_IS_PRIVATE_KEY (object)) {
		private_key = SEAHORSE_PRIVATE_KEY (object);
		pair = seahorse_private_key_get_partner (private_key);
		seahorse_private_key_set_partner (private_key, NULL);
	} else {
		pair = NULL;
	}

	return pair;
}

static void
receive_objects (SeahorseToken *self,
                 GList *objects)
{
	GckAttributes *attrs;
	const GckAttribute *id;
	gpointer object;
	gpointer prev;
	gpointer pair;
	gulong handle;
	GList *show = NULL;
	GList *hide = NULL;
	GList *l;

	for (l = objects; l != NULL; l = g_list_next (l)) {
		object = l->data;
		handle = gck_object_get_handle (object);
		attrs = gck_object_cache_get_attributes (object);

		prev = g_hash_table_lookup (self->pv->object_for_handle, &handle);
		if (prev == NULL) {
			g_hash_table_insert (self->pv->object_for_handle,
			                     g_memdup (&handle, sizeof (handle)),
			                     g_object_ref (object));
			g_object_set (object, "place", self, NULL);
		} else if (prev != object) {
			gck_object_cache_set_attributes (prev, attrs);
			object = prev;
		}

		id = attrs ? gck_attributes_find (attrs, CKA_ID) : NULL;
		update_id_map (self, object, id);

		if (SEAHORSE_IS_CERTIFICATE (object)) {
			pair = lookup_id_map (self, SEAHORSE_TYPE_PRIVATE_KEY, id);
			if (pair && make_certificate_key_pair (object, pair))
				hide = g_list_prepend (hide, pair);
			show = g_list_prepend (show, object);

		} else if (SEAHORSE_IS_PRIVATE_KEY (object)) {
			pair = lookup_id_map (self, SEAHORSE_TYPE_CERTIFICATE, id);
			if (pair && make_certificate_key_pair (pair, object))
				hide = g_list_prepend (hide, object);
			else
				show = g_list_prepend (show, object);

		} else {
			show = g_list_prepend (show, object);
		}

		gck_attributes_unref (attrs);
	}

	update_visibility (self, hide, FALSE);
	g_list_free (hide);

	update_visibility (self, show, TRUE);
	g_list_free (show);
}

static void
remove_objects (SeahorseToken *self,
                GList *objects)
{
	GList *depaired = NULL;
	GList *hide = NULL;
	gulong handle;
	gpointer object;
	gpointer pair;
	GList *l;

	for (l = objects; l != NULL; l = g_list_next (l)) {
		object = l->data;
		pair = break_certificate_key_pair (object);
		if (pair != NULL)
			depaired = g_list_prepend (depaired, pair);
		update_id_map (self, object, NULL);
		hide = g_list_prepend (hide, object);
	}

	/* Remove the ownership of these */
	for (l = objects; l != NULL; l = g_list_next (l)) {
		handle = gck_object_get_handle (l->data);
		g_object_set (object, "place", NULL, NULL);
		g_hash_table_remove (self->pv->object_for_handle, &handle);
	}

	update_visibility (self, hide, FALSE);
	g_list_free (hide);

	/* Add everything that was paired */
	receive_objects (self, depaired);
	g_list_free (depaired);
}


typedef struct {
	SeahorseToken *token;
	GCancellable *cancellable;
	GHashTable *checks;
	GckEnumerator *enumerator;
} RefreshClosure;

static void
pkcs11_refresh_free (gpointer data)
{
	RefreshClosure *closure = data;
	g_object_unref (closure->token);
	g_clear_object (&closure->cancellable);
	g_hash_table_destroy (closure->checks);
	g_clear_object (&closure->enumerator);
	g_free (closure);
}

static void
on_refresh_next_objects (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	RefreshClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	gulong handle;
	GList *objects;
	GList *l;

	objects = gck_enumerator_next_finish (closure->enumerator, result, &error);

	receive_objects (closure->token, objects);

	/* Remove all objects that were found from the check table */
	for (l = objects; l; l = g_list_next (l)) {
		handle = gck_object_get_handle (l->data);
		g_hash_table_remove (closure->checks, &handle);
	}

	gck_list_unref_free (objects);

	/* If there was an error, report it */
	if (error != NULL) {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);

	/* Otherwise if there were objects go back and get more */
	} else if (objects != NULL) {
		gck_enumerator_next_async (closure->enumerator, 16, closure->cancellable,
		                           on_refresh_next_objects, g_object_ref (res));

	/* Otherwise we're done, remove everything not found */
	} else {
		objects = g_hash_table_get_values (closure->checks);
		remove_objects (closure->token, objects);
		g_list_free (objects);

		g_simple_async_result_complete (res);
	}

	g_object_unref (res);
}

static void
refresh_enumerate (GSimpleAsyncResult *res)
{
	RefreshClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GckBuilder builder = GCK_BUILDER_INIT;
	GckSession *session;
	GckEnumerator *enumerator;

	session = seahorse_token_get_session (closure->token);

	gck_builder_add_boolean (&builder, CKA_TOKEN, TRUE);
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_CERTIFICATE);
	enumerator = gck_session_enumerate_objects (session, gck_builder_end (&builder));
	gck_enumerator_set_object_type (enumerator, SEAHORSE_TYPE_CERTIFICATE);
	closure->enumerator = enumerator;

	gck_builder_add_boolean (&builder, CKA_TOKEN, TRUE);
	gck_builder_add_ulong (&builder, CKA_CLASS, CKO_PRIVATE_KEY);
	enumerator = gck_session_enumerate_objects (session, gck_builder_end (&builder));
	gck_enumerator_set_object_type (enumerator, SEAHORSE_TYPE_PRIVATE_KEY);
	gck_enumerator_set_chained (closure->enumerator, enumerator);
	g_object_unref (enumerator);

	gck_enumerator_next_async (closure->enumerator, 16, closure->cancellable,
	                           on_refresh_next_objects, g_object_ref (res));
}

static void
on_refresh_session_open (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	RefreshClosure *closure = g_simple_async_result_get_op_res_gpointer (res);
	GError *error = NULL;
	GckSession *session;

	session = gck_session_open_finish (result, &error);

	if (error == NULL) {
		seahorse_token_set_session (closure->token, session);
		refresh_enumerate (res);
		g_object_unref (session);
	} else {
		g_simple_async_result_take_error (res, error);
		g_simple_async_result_complete (res);
	}

	g_object_unref (res);
}

static GckSessionOptions
calculate_session_options (SeahorseToken *self)
{
	GckTokenInfo *info = seahorse_token_get_info (self);
	if (info->flags & CKF_WRITE_PROTECTED)
		return GCK_SESSION_READ_ONLY;
	else
		return GCK_SESSION_READ_WRITE;
}

static void
seahorse_token_load_async (SeahorsePlace *place,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	SeahorseToken *self = SEAHORSE_TOKEN (place);
	GSimpleAsyncResult *res;
	RefreshClosure *closure;
	GckSessionOptions options;
	GList *objects, *l;
	gulong handle;

	update_token_info (self);

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_token_load_async);
	closure = g_new0 (RefreshClosure, 1);
	closure->checks = g_hash_table_new_full (seahorse_pkcs11_ulong_hash,
	                                         seahorse_pkcs11_ulong_equal,
	                                         g_free, g_object_unref);
	closure->token = g_object_ref (self);
	closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	g_simple_async_result_set_op_res_gpointer (res, closure, pkcs11_refresh_free);

	/* Make note of all the objects that were there */
	objects = gcr_collection_get_objects (GCR_COLLECTION (self));
	for (l = objects; l; l = g_list_next (l)) {
		handle = gck_object_get_handle (l->data);
		g_hash_table_insert (closure->checks,
		                     g_memdup (&handle, sizeof (handle)),
		                     g_object_ref (l->data));
	}
	g_list_free (objects);

	if (!self->pv->session) {
		options = calculate_session_options (self);
		gck_session_open_async (self->pv->slot, options, NULL, cancellable,
		                        on_refresh_session_open, g_object_ref (res));
	} else {
		refresh_enumerate (res);
	}

	g_object_unref (res);
}

static gboolean
seahorse_token_load_finish (SeahorsePlace *place,
                            GAsyncResult *result,
                            GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (place),
	                      seahorse_token_load_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;

}

static void
seahorse_token_init (SeahorseToken *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_TOKEN, SeahorseTokenPrivate));
	self->pv->object_for_handle = g_hash_table_new_full (seahorse_pkcs11_ulong_hash,
	                                                     seahorse_pkcs11_ulong_equal,
	                                                     g_free, g_object_unref);
	self->pv->objects_for_id = g_hash_table_new_full (gck_attribute_hash,
	                                                  gck_attribute_equal,
	                                                  gck_attribute_free,
	                                                  (GDestroyNotify)g_ptr_array_unref);
	self->pv->id_for_object = g_hash_table_new (g_direct_hash, g_direct_equal);
	self->pv->objects_visible = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
seahorse_token_constructed (GObject *obj)
{
	SeahorseToken *self = SEAHORSE_TOKEN (obj);
	GckUriData *data;

	G_OBJECT_CLASS (seahorse_token_parent_class)->constructed (obj);

	g_return_if_fail (self->pv->slot != NULL);

	seahorse_place_load_async (SEAHORSE_PLACE (self), NULL, NULL, NULL);

	data = gck_uri_data_new ();
	data->token_info = seahorse_token_get_info (self);
	self->pv->uri = gck_uri_build (data, GCK_URI_FOR_TOKEN);
	data->token_info = NULL;
	gck_uri_data_free (data);
}

static void
seahorse_token_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	SeahorseToken *self = SEAHORSE_TOKEN (object);
	GckTokenInfo *token;

	switch (prop_id) {
	case PROP_LABEL:
		token = gck_slot_get_token_info (self->pv->slot);
		if (token == NULL)
			g_value_set_string (value, C_("Label", "Unknown"));
		else
			g_value_set_string (value, token->label);
		gck_token_info_free (token);
		break;
	case PROP_DESCRIPTION:
		token = gck_slot_get_token_info (self->pv->slot);
		if (token == NULL)
			g_value_set_string (value, NULL);
		else
			g_value_set_string (value, token->manufacturer_id);
		gck_token_info_free (token);
		break;
	case PROP_ICON:
		token = gck_slot_get_token_info (self->pv->slot);
		if (token == NULL)
			g_value_take_object (value, g_themed_icon_new (GTK_STOCK_DIALOG_QUESTION));
		else
			g_value_take_object (value, gcr_icon_for_token (token));
		gck_token_info_free (token);
		break;
	case PROP_SLOT:
		g_value_set_object (value, self->pv->slot);
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, 0);
		break;
	case PROP_URI:
		g_value_set_string (value, self->pv->uri);
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, NULL);
		break;
	case PROP_INFO:
		g_value_set_boxed (value, self->pv->info);
		break;
	case PROP_LOCKABLE:
		g_value_set_boolean (value, seahorse_token_get_lockable (self));
		break;
	case PROP_UNLOCKABLE:
		g_value_set_boolean (value, seahorse_token_get_unlockable (self));
		break;
	case PROP_SESSION:
		g_value_set_object (value, seahorse_token_get_session (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
seahorse_token_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	SeahorseToken *self = SEAHORSE_TOKEN (object);

	switch (prop_id) {
	case PROP_SLOT:
		g_return_if_fail (!self->pv->slot);
		self->pv->slot = g_value_get_object (value);
		g_return_if_fail (self->pv->slot);
		g_object_ref (self->pv->slot);
		break;
	case PROP_INFO:
		g_return_if_fail (!self->pv->info);
		self->pv->info = g_value_dup_boxed (value);
		break;
	case PROP_SESSION:
		seahorse_token_set_session (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	};
}

static void
seahorse_token_dispose (GObject *obj)
{
	SeahorseToken *self = SEAHORSE_TOKEN (obj);

	g_clear_object (&self->pv->slot);
	g_clear_object (&self->pv->session);

	G_OBJECT_CLASS (seahorse_token_parent_class)->dispose (obj);
}

static void
seahorse_token_finalize (GObject *obj)
{
	SeahorseToken *self = SEAHORSE_TOKEN (obj);

	g_hash_table_destroy (self->pv->objects_visible);
	g_hash_table_destroy (self->pv->object_for_handle);
	g_hash_table_destroy (self->pv->objects_for_id);
	g_hash_table_destroy (self->pv->id_for_object);
	g_assert (self->pv->slot == NULL);
	g_free (self->pv->uri);

	G_OBJECT_CLASS (seahorse_token_parent_class)->finalize (obj);
}

static void
seahorse_token_class_init (SeahorseTokenClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorseTokenPrivate));

	gobject_class->constructed = seahorse_token_constructed;
	gobject_class->dispose = seahorse_token_dispose;
	gobject_class->finalize = seahorse_token_finalize;
	gobject_class->set_property = seahorse_token_set_property;
	gobject_class->get_property = seahorse_token_get_property;

	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
	g_object_class_override_property (gobject_class, PROP_URI, "uri");
	g_object_class_override_property (gobject_class, PROP_ICON, "icon");
	g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");

	g_object_class_install_property (gobject_class, PROP_SLOT,
	         g_param_spec_object ("slot", "Slot", "Pkcs#11 SLOT",
	                              GCK_TYPE_SLOT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_uint ("object-flags", "Flags", "Object Token flags.",
	                            0, G_MAXUINT, 0, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_INFO,
	         g_param_spec_boxed ("info", "Info", "Token info",
	                             GCK_TYPE_TOKEN_INFO, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_LOCKABLE,
	         g_param_spec_boolean ("lockable", "Lockable", "Token can be locked",
	                               FALSE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_UNLOCKABLE,
	         g_param_spec_boolean ("unlockable", "Unlockable", "Token can be unlocked",
	                               FALSE, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_SLOT,
	            g_param_spec_object ("session", "Session", "Pkcs#11 Session",
	                                 GCK_TYPE_SESSION, G_PARAM_READWRITE));
}

static void
seahorse_token_place_iface (SeahorsePlaceIface *iface)
{
	iface->load_async = seahorse_token_load_async;
	iface->load_finish = seahorse_token_load_finish;
}

static guint
seahorse_token_get_length (GcrCollection *collection)
{
	SeahorseToken *self = SEAHORSE_TOKEN (collection);
	return g_hash_table_size (self->pv->objects_visible);
}

static GList *
seahorse_token_get_objects (GcrCollection *collection)
{
	SeahorseToken *self = SEAHORSE_TOKEN (collection);
	return g_hash_table_get_values (self->pv->objects_visible);
}

static gboolean
seahorse_token_contains (GcrCollection *collection,
                         GObject *object)
{
	SeahorseToken *self = SEAHORSE_TOKEN (collection);
	return g_hash_table_lookup (self->pv->objects_visible, object) != NULL;
}

static void
seahorse_token_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = seahorse_token_get_length;
	iface->get_objects = seahorse_token_get_objects;
	iface->contains = seahorse_token_contains;
}

static gboolean
is_session_logged_in (GckSession *session)
{
	GckSessionInfo *info;
	gboolean logged_in;

	if (session == NULL)
		return FALSE;

	info = gck_session_get_info (session);
	logged_in = (info != NULL) &&
	            (info->state == CKS_RW_USER_FUNCTIONS ||
	             info->state == CKS_RO_USER_FUNCTIONS ||
	             info->state == CKS_RW_SO_FUNCTIONS);
	gck_session_info_free (info);

	return logged_in;
}

static void
on_session_logout (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseToken *self = SEAHORSE_TOKEN (g_async_result_get_source_object (user_data));
	GError *error = NULL;

	gck_session_logout_finish (GCK_SESSION (source), result, &error);
	if (error == NULL)
		seahorse_place_load_async (SEAHORSE_PLACE (self), NULL, NULL, NULL);
	else
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (self);
	g_object_unref (res);
}

static void
on_login_interactive (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseToken *self = SEAHORSE_TOKEN (g_async_result_get_source_object (user_data));
	GError *error = NULL;

	gck_session_logout_finish (GCK_SESSION (source), result, &error);
	if (error == NULL)
		seahorse_token_load_async (SEAHORSE_PLACE (self), NULL, NULL, NULL);
	else
		g_simple_async_result_take_error (res, error);

	g_simple_async_result_complete (res);
	g_object_unref (self);
	g_object_unref (res);
}

static void
on_session_login_open (GObject *source,
                       GAsyncResult *result,
                       gpointer user_data)
{
	GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (user_data);
	SeahorseToken *self = SEAHORSE_TOKEN (g_async_result_get_source_object (user_data));
	GckSession *session;
	GError *error = NULL;

	session = gck_session_open_finish (result, &error);
	if (error == NULL) {
		seahorse_token_set_session (self, session);
		seahorse_place_load_async (SEAHORSE_PLACE (self), NULL, NULL, NULL);
		g_object_unref (session);
	} else {
		g_simple_async_result_take_error (res, error);
	}

	g_simple_async_result_complete (res);
	g_object_unref (self);
	g_object_unref (res);
}

static void
seahorse_token_lock_async (SeahorseLockable *lockable,
                           GTlsInteraction *interaction,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	SeahorseToken *self = SEAHORSE_TOKEN (lockable);
	GSimpleAsyncResult *res;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_token_lock_async);

	if (is_session_logged_in (self->pv->session))
		gck_session_logout_async (self->pv->session, cancellable,
		                          on_session_logout, g_object_ref (res));
	else
		g_simple_async_result_complete_in_idle (res);

	g_object_unref (res);
}

static gboolean
seahorse_token_lock_finish (SeahorseLockable *lockable,
                            GAsyncResult *result,
                            GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (lockable),
	                      seahorse_token_lock_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
seahorse_token_unlock_async (SeahorseLockable *lockable,
                             GTlsInteraction *interaction,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	SeahorseToken *self = SEAHORSE_TOKEN (lockable);
	GSimpleAsyncResult *res;
	GckSessionOptions options;

	res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
	                                 seahorse_token_unlock_async);

	if (is_session_logged_in (self->pv->session)) {
		g_simple_async_result_complete_in_idle (res);

	} else if (self->pv->session) {
		gck_session_login_interactive_async (self->pv->session, CKU_USER,
		                                     interaction, NULL, on_login_interactive,
		                                     g_object_ref (res));
	} else {
		options = calculate_session_options (self);
		gck_session_open_async (self->pv->slot, options | GCK_SESSION_LOGIN_USER, interaction,
		                        NULL, on_session_login_open, g_object_ref (res));
	}

	g_object_unref (res);
}

static gboolean
seahorse_token_unlock_finish (SeahorseLockable *lockable,
                              GAsyncResult *result,
                              GError **error)
{
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (lockable),
	                      seahorse_token_unlock_async), FALSE);

	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
		return FALSE;

	return TRUE;
}

static void
seahorse_token_lockable_iface (SeahorseLockableIface *iface)
{
	iface->lock_async = seahorse_token_lock_async;
	iface->lock_finish = seahorse_token_lock_finish;
	iface->unlock_async = seahorse_token_unlock_async;
	iface->unlock_finish = seahorse_token_unlock_finish;
}

SeahorseToken *
seahorse_token_new (GckSlot *slot)
{
	return g_object_new (SEAHORSE_TYPE_TOKEN,
	                     "slot", slot,
	                     NULL);
}

GckSlot *
seahorse_token_get_slot (SeahorseToken *self)
{
	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), NULL);
	return self->pv->slot;
}

GckSession *
seahorse_token_get_session (SeahorseToken *self)
{
	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), NULL);
	return self->pv->session;
}

void
seahorse_token_set_session (SeahorseToken *self,
                            GckSession *session)
{
	GObject *obj;

	g_return_if_fail (SEAHORSE_IS_TOKEN (self));
	g_return_if_fail (session == NULL || GCK_IS_SESSION (session));

	if (session)
		g_object_ref (session);
	g_clear_object (&self->pv->session);
	self->pv->session = session;

	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	g_object_notify (obj, "session");
	g_object_notify (obj, "lockable");
	g_object_notify (obj, "unlockable");
	g_object_thaw_notify (obj);
}

GckTokenInfo *
seahorse_token_get_info (SeahorseToken *self)
{
	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), NULL);

	if (self->pv->info == NULL)
		update_token_info (self);

	return self->pv->info;
}

gboolean
seahorse_token_get_lockable (SeahorseToken *self)
{
	GckTokenInfo *info;

	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), FALSE);

	info = seahorse_token_get_info (self);

	if ((info->flags & CKF_LOGIN_REQUIRED) == 0)
		return FALSE;

	if ((info->flags & CKF_USER_PIN_INITIALIZED) == 0)
		return FALSE;

	return is_session_logged_in (self->pv->session);

}

gboolean
seahorse_token_get_unlockable (SeahorseToken *self)
{
	GckTokenInfo *info;

	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), FALSE);

	info = seahorse_token_get_info (self);

	if ((info->flags & CKF_LOGIN_REQUIRED) == 0)
		return FALSE;

	if ((info->flags & CKF_USER_PIN_INITIALIZED) == 0)
		return FALSE;

	return !is_session_logged_in (self->pv->session);
}

GArray *
seahorse_token_get_mechanisms (SeahorseToken *self)
{
	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), NULL);

	if (!self->pv->mechanisms)
		update_mechanisms (self);

	return self->pv->mechanisms;
}

gboolean
seahorse_token_has_mechanism (SeahorseToken *self,
                              gulong mechanism)
{
	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), FALSE);

	if (!self->pv->mechanisms)
		update_mechanisms (self);

	return gck_mechanisms_check (self->pv->mechanisms,
	                             mechanism,
	                             GCK_INVALID);
}

void
seahorse_token_remove_object (SeahorseToken *self,
                              GckObject *object)
{
	GList *objects;

	g_return_if_fail (SEAHORSE_IS_TOKEN (self));
	g_return_if_fail (GCK_IS_OBJECT (object));

	objects = g_list_prepend (NULL, g_object_ref (object));
	remove_objects (self, objects);
	g_list_free_full (objects, g_object_unref);
}

gboolean
seahorse_token_is_deletable (SeahorseToken *self,
                             GckObject *object)
{
	GckAttributes *attrs;
	GckTokenInfo *info;
	gboolean ret;

	g_return_val_if_fail (SEAHORSE_IS_TOKEN (self), FALSE);
	g_return_val_if_fail (GCK_IS_OBJECT (object), FALSE);

	info = seahorse_token_get_info (self);
	if (info->flags & CKF_WRITE_PROTECTED)
		return FALSE;

	g_object_get (object, "attributes", &attrs, NULL);

	if (!gck_attributes_find_boolean (attrs, CKA_MODIFIABLE, &ret))
		ret = TRUE;

	gck_attributes_unref (attrs);
	return ret;
}
