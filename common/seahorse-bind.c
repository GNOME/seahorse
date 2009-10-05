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

#include "seahorse-bind.h"

#include <string.h>

static gboolean
value_equal (const GValue *a, const GValue *b)
{
	gboolean retval;
	const gchar *stra, *strb;

	if (G_VALUE_TYPE (a) != G_VALUE_TYPE (b))
		return FALSE;
	
	switch (G_VALUE_TYPE (a))
	{
	case G_TYPE_BOOLEAN:
		if (g_value_get_boolean (a) < g_value_get_boolean (b))
			retval = FALSE;
		else if (g_value_get_boolean (a) == g_value_get_boolean (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_CHAR:
		if (g_value_get_char (a) < g_value_get_char (b))
			retval = FALSE;
		else if (g_value_get_char (a) == g_value_get_char (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_UCHAR:
		if (g_value_get_uchar (a) < g_value_get_uchar (b))
			retval = FALSE;
		else if (g_value_get_uchar (a) == g_value_get_uchar (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_INT:
		if (g_value_get_int (a) < g_value_get_int (b))
			retval = FALSE;
		else if (g_value_get_int (a) == g_value_get_int (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_UINT:
		if (g_value_get_uint (a) < g_value_get_uint (b))
			retval = FALSE;
		else if (g_value_get_uint (a) == g_value_get_uint (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_LONG:
		if (g_value_get_long (a) < g_value_get_long (b))
			retval = FALSE;
		else if (g_value_get_long (a) == g_value_get_long (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_ULONG:
		if (g_value_get_ulong (a) < g_value_get_ulong (b))
			retval = FALSE;
		else if (g_value_get_ulong (a) == g_value_get_ulong (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_INT64:
		if (g_value_get_int64 (a) < g_value_get_int64 (b))
			retval = FALSE;
		else if (g_value_get_int64 (a) == g_value_get_int64 (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_UINT64:
		if (g_value_get_uint64 (a) < g_value_get_uint64 (b))
			retval = FALSE;
		else if (g_value_get_uint64 (a) == g_value_get_uint64 (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_FLOAT:
		if (g_value_get_float (a) < g_value_get_float (b))
			retval = FALSE;
		else if (g_value_get_float (a) == g_value_get_float (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_DOUBLE:
		if (g_value_get_double (a) < g_value_get_double (b))
			retval = FALSE;
		else if (g_value_get_double (a) == g_value_get_double (b))
			retval = TRUE;
		else
			retval = FALSE;
		break;
	case G_TYPE_STRING:
		stra = g_value_get_string (a);
		strb = g_value_get_string (b);
		if (stra == strb)
			retval = TRUE;
		else if (!stra || !strb)
			retval = FALSE;
		else
			retval = g_utf8_collate (stra, strb) == 0;
		break;
	case G_TYPE_POINTER:
		retval = (g_value_get_pointer (a) == g_value_get_pointer (b));
		break;
	case G_TYPE_BOXED:
		retval = (g_value_get_boxed (a) == g_value_get_boxed (b));
		break;
	case G_TYPE_OBJECT:
		retval = (g_value_get_object (a) == g_value_get_object (b));
		break;
	default:
        if (G_VALUE_HOLDS_ENUM (a)) {
            retval = (g_value_get_enum (a) == g_value_get_enum (b));
        } else if (G_VALUE_HOLDS_FLAGS (a)) {
            retval = (g_value_get_flags (a) == g_value_get_flags (b));
        } else {
            /* Default case is not equal */
            retval = FALSE;
        }
		break;
	}
	
	return retval;
}

static GHashTable *all_bindings = NULL;

typedef struct _Binding {
	GObject *obj_src;
	GParamSpec *prop_src;
	GParamSpec *prop_dest;
	GSList *obj_dests;
	
	gulong connection;
	SeahorseTransform transform;
	gboolean processing;
	
	gint references;
} Binding;

static void binding_unref (Binding *binding);
static void binding_ref (Binding *binding);

static void 
binding_src_gone (gpointer data, GObject *was)
{
	Binding *binding = (Binding*)data;
	g_assert (binding->obj_src == was);
	binding->obj_src = NULL;
	binding_unref (binding);
}

static void
binding_dest_gone (gpointer data, GObject *was)
{
	Binding *binding = (Binding*)data;
	GSList *at;

	at = g_slist_find (binding->obj_dests, was);
	g_assert (at != NULL);
	
	/* Remove it from the list */
	binding->obj_dests = g_slist_delete_link (binding->obj_dests, at);
	
	/* If no more destination objects, then go away */
	if (!binding->obj_dests)
		binding_unref (binding);
}

static void 
binding_ref (Binding *binding)
{
	g_assert (binding);
	++binding->references;
}

static void
binding_unref (Binding *binding)
{
	GSList *l;

	g_assert (binding);
	
	g_assert (binding->references > 0);
	--binding->references;
	if (binding->references > 0)
		return;
	
	if (G_IS_OBJECT (binding->obj_src)) {
		g_signal_handler_disconnect (binding->obj_src, binding->connection);
		g_object_weak_unref (binding->obj_src, binding_src_gone, binding);
		binding->obj_src = NULL;
	}
	
	for (l = binding->obj_dests; l; l = g_slist_next (l)) {
		if (G_IS_OBJECT (l->data))
			g_object_weak_unref (l->data, binding_dest_gone, binding);
	}
	g_slist_free (binding->obj_dests);
	binding->obj_dests = NULL;
	
	g_assert (binding->prop_src);
	g_param_spec_unref (binding->prop_src);
	binding->prop_src = NULL;
	
	g_assert (binding->prop_dest);
	g_param_spec_unref (binding->prop_dest);
	binding->prop_dest = NULL;
	
	g_free (binding);
	
	/* Remove from the list of all bindings */
	g_assert (all_bindings);
	g_hash_table_remove (all_bindings, binding);
	if (g_hash_table_size (all_bindings) == 0) {
		g_hash_table_destroy (all_bindings);
		all_bindings = NULL;
	}
}

static void
bind_transfer (Binding *binding, gboolean forward)
{
	GValue src, dest, check;
	GSList *l;
	
	g_assert (binding->obj_src);
	g_assert (binding->obj_dests);
	
	/* IMPORTANT: No return during this fuction */
	binding_ref (binding);
	binding->processing = TRUE;

	/* Get the value from the source object */
	memset (&src, 0, sizeof (src));
	g_value_init (&src, binding->prop_src->value_type);
	g_object_get_property (binding->obj_src, binding->prop_src->name, &src);
	
	/* Transform the value */
	memset (&dest, 0, sizeof (dest));
	g_value_init (&dest, binding->prop_dest->value_type);
	if ((binding->transform) (&src, &dest)) {


		for (l = binding->obj_dests; l; l = g_slist_next (l)) {

			/* Get the current value of the destination object */
			memset (&check, 0, sizeof (check));
			g_value_init (&check, binding->prop_dest->value_type);
			g_object_get_property (l->data, binding->prop_dest->name, &check);
			
			/* Set the property on the destination object */
			if (!value_equal (&dest, &check))
				g_object_set_property (l->data, binding->prop_dest->name, &dest);

			g_value_unset (&check);
		}

	} else {
		
		g_warning ("couldn't transform value from '%s' to '%s' when trying to "
		           "transfer bound property from %s.%s to %s.%s",
		           g_type_name (binding->prop_src->value_type), 
		           g_type_name (binding->prop_dest->value_type),
		           G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (binding->obj_src)), 
		           binding->prop_src->name,
		           G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (binding->obj_dests->data)), 
		           binding->prop_dest->name);
	}
	
	g_value_unset (&src);
	g_value_unset (&dest);
	
	binding->processing = FALSE;
	binding_unref (binding);
}

static void 
binding_fire (GObject *gobject, GParamSpec *pspec, gpointer data)
{
	Binding *binding = (Binding*)data;
	g_assert (gobject == binding->obj_src);
	g_assert (pspec == binding->prop_src);
	g_assert (binding->transform);
	
	if (!binding->processing)
		bind_transfer (binding, TRUE);
}

gpointer 
seahorse_bind_property (const gchar *prop_src, gpointer obj_src, 
                        const gchar *prop_dest, gpointer obj_dest)
{
	g_return_val_if_fail (G_IS_OBJECT (obj_src), NULL);
	g_return_val_if_fail (prop_src, NULL);
	g_return_val_if_fail (G_IS_OBJECT (obj_dest), NULL);
	g_return_val_if_fail (prop_dest, NULL);
	
	return seahorse_bind_property_full (prop_src, obj_src,
	                                    g_value_transform,
	                                    prop_dest, obj_dest, NULL);
}

gpointer 
seahorse_bind_property_full (const gchar *prop_src, gpointer obj_src,
                             SeahorseTransform transform,
                             const gchar *prop_dest, ...)
{
	GObjectClass *cls;
	GParamSpec *spec_src;
	GParamSpec *spec_dest;
	GParamSpec *spec;
	Binding *binding;
	gchar *detail;
	GObject *dest;
	GSList *dests, *l;
	va_list va;

	g_return_val_if_fail (transform, NULL);
	g_return_val_if_fail (G_IS_OBJECT (obj_src), NULL);
	g_return_val_if_fail (prop_src, NULL);
	g_return_val_if_fail (prop_dest, NULL);
	
	cls = G_OBJECT_GET_CLASS (obj_src);
	spec_src = g_object_class_find_property (cls, prop_src);
	if (!spec_src) {
		g_warning ("no property with the name '%s' exists in object of class '%s'",
		           prop_src, G_OBJECT_CLASS_NAME (cls));
		return NULL;
	}
	
	dests = NULL;
	spec_dest = NULL;
	va_start(va, prop_dest);
	for (;;) {
		dest = G_OBJECT (va_arg (va, GObject*));
		if (!dest)
			break;
		
		g_return_val_if_fail (G_IS_OBJECT (dest), NULL);
		
		cls = G_OBJECT_GET_CLASS (dest);
		spec = g_object_class_find_property (cls, prop_dest);
		if (!spec) {
			g_warning ("no property with the name '%s' exists in object of class '%s'",
			           prop_dest, G_OBJECT_CLASS_NAME (cls));
			return NULL;
		} 
		
		if (spec_dest && spec->value_type != spec_dest->value_type) {
			g_warning ("destination property '%s' has a different type between objects in binding: %s != %s",
			           prop_dest, g_type_name (spec_dest->value_type), g_type_name (spec->value_type));
			return NULL;
		} 
		
		dests = g_slist_prepend (dests, dest);
		spec_dest = spec;
	}

	g_return_val_if_fail (spec_dest, NULL);
	g_return_val_if_fail (dests, NULL);
	
	binding = g_new0 (Binding, 1);

	binding->obj_src = obj_src;
	g_object_weak_ref (obj_src, binding_src_gone, binding);
	binding->prop_src = spec_src;
	g_param_spec_ref (spec_src);
	binding->transform = transform;
	detail = g_strdup_printf ("notify::%s", prop_src);
	binding->connection = g_signal_connect (obj_src, detail, G_CALLBACK (binding_fire), binding);
	g_free (detail);
	
	binding->obj_dests = dests;
	binding->prop_dest = spec_dest;
	g_param_spec_ref (spec_dest);
	for (l = binding->obj_dests; l; l = g_slist_next (l))
		g_object_weak_ref (l->data, binding_dest_gone, binding);

	binding->references = 1;
	
	/* Note this in all bindings */
	if (!all_bindings)
		all_bindings = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_hash_table_insert (all_bindings, binding, binding);
	
	/* Transfer the first time */
	binding_fire (obj_src, spec_src, binding);
	
	return binding;
}

static GHashTable *all_transfers = NULL;

typedef struct _Transfer {
	GObject *object;
	GObject *dest;
	gulong connection;
	SeahorseTransfer callback;
} Transfer;

static void transfer_free (Transfer *transfer);

static void
transfer_gone (gpointer data, GObject *was)
{
	Transfer *transfer = (Transfer*)data;
	if (transfer->object == was)
		transfer->object = NULL;
	else if (transfer->dest == was)
		transfer->dest = NULL;
	else
		g_assert_not_reached ();
	transfer_free (transfer);
}

static void 
transfer_free (Transfer *transfer)
{
	g_assert (transfer);
	
	if (G_IS_OBJECT (transfer->object)) {
		g_object_weak_unref (transfer->object, transfer_gone, transfer);
		g_signal_handler_disconnect (transfer->object, transfer->connection);
		transfer->object = NULL;
	}
	
	if (G_IS_OBJECT (transfer->dest)) {
		g_object_weak_unref (transfer->dest, transfer_gone, transfer);
		transfer->dest = NULL;
	}
	
	g_free (transfer);
	
	/* Remove from the list of all notifies */
	g_assert (all_transfers);
	g_hash_table_remove (all_transfers, transfer);
	if (g_hash_table_size (all_transfers) == 0) {
		g_hash_table_destroy (all_transfers);
		all_transfers = NULL;
	}
}

static void
transfer_fire (GObject *object, GParamSpec *spec, gpointer data)
{
	Transfer *transfer = (Transfer*)data;
	g_assert (transfer->object == object);
	(transfer->callback) (object, transfer->dest);
}

gpointer 
seahorse_bind_objects (const gchar *property, gpointer object, 
                       SeahorseTransfer callback, gpointer dest)
{
	GObjectClass *cls;
	Transfer *transfer;
	gchar *detail;

	g_return_val_if_fail (G_IS_OBJECT (object), NULL);
	g_return_val_if_fail (G_IS_OBJECT (dest), NULL);
	g_return_val_if_fail (callback, NULL);
	
	if (property) {
		cls = G_OBJECT_GET_CLASS (object);
		if (!g_object_class_find_property (cls, property)) {
			g_warning ("no property with the name '%s' exists in object of class '%s'",
			           property, G_OBJECT_CLASS_NAME (cls));
			return NULL;
		}
	}
	
	transfer = g_new0 (Transfer, 1);

	transfer->object = object;
	g_object_weak_ref (object, transfer_gone, transfer);
	transfer->dest = dest;
	g_object_weak_ref (dest, transfer_gone, transfer);
	transfer->callback = callback;
	
	if (property) {
		detail = g_strdup_printf ("notify::%s", property);
		transfer->connection = g_signal_connect (object, detail, G_CALLBACK (transfer_fire), transfer);
		g_free (detail);
	} else {
		transfer->connection = g_signal_connect (object, "notify", G_CALLBACK (transfer_fire), transfer);
	}
	
	/* Note this in all bindings */
	if (!all_transfers)
		all_transfers = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_hash_table_insert (all_transfers, transfer, transfer);
	
	/* Transfer the first time */
	transfer_fire (object, NULL, transfer);
	
	return transfer;
}

void
seahorse_bind_disconnect (gpointer what)
{
	g_return_if_fail (what);
	if (all_bindings && g_hash_table_lookup (all_bindings, what))
		binding_unref ((Binding*)what);
	else if (all_transfers && g_hash_table_lookup (all_transfers, what))
		transfer_free ((Transfer*)what);
}
