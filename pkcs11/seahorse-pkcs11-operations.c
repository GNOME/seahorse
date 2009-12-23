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

#include "seahorse-pkcs11-object.h"
#include "seahorse-pkcs11-operations.h"
#include "seahorse-pkcs11-source.h"

#include "common/seahorse-object-list.h"

#include <pkcs11.h>
#include <pkcs11g.h>
#include <gp11.h>

#include <glib/gi18n.h>

static void 
seahorse_pkcs11_mark_complete (SeahorseOperation *self, GError *error) 
{
	SeahorseOperation *operation = SEAHORSE_OPERATION (self);
	if (error == NULL) 
		seahorse_operation_mark_done (operation, FALSE, NULL);
	else if (error->code == CKR_FUNCTION_CANCELED)
		seahorse_operation_mark_done (operation, TRUE, NULL);
	else 	
		seahorse_operation_mark_done (operation, FALSE, error);
	g_clear_error (&error);
}

/* -----------------------------------------------------------------------------
 * REFRESHER OPERATION
 */

#define SEAHORSE_TYPE_PKCS11_REFRESHER               (seahorse_pkcs11_refresher_get_type ())
#define SEAHORSE_PKCS11_REFRESHER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_REFRESHER, SeahorsePkcs11Refresher))
#define SEAHORSE_PKCS11_REFRESHER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_REFRESHER, SeahorsePkcs11RefresherClass))
#define SEAHORSE_IS_PKCS11_REFRESHER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_REFRESHER))
#define SEAHORSE_IS_PKCS11_REFRESHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_REFRESHER))
#define SEAHORSE_PKCS11_REFRESHER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_REFRESHER, SeahorsePkcs11RefresherClass))

typedef struct _SeahorsePkcs11Refresher SeahorsePkcs11Refresher;
typedef struct _SeahorsePkcs11RefresherClass SeahorsePkcs11RefresherClass;
    
struct _SeahorsePkcs11Refresher {
	SeahorseOperation parent;
	GCancellable *cancellable;
	SeahorsePkcs11Source *source;
	GP11Session *session;
	GHashTable *checks;
};

struct _SeahorsePkcs11RefresherClass {
	SeahorseOperationClass parent_class;
};

enum {
	PROP_0,
	PROP_SOURCE
};

G_DEFINE_TYPE (SeahorsePkcs11Refresher, seahorse_pkcs11_refresher, SEAHORSE_TYPE_OPERATION);

static guint
ulong_hash (gconstpointer k)
{
	return (guint)*((gulong*)k); 
}

static gboolean
ulong_equal (gconstpointer a, gconstpointer b)
{
	return *((gulong*)a) == *((gulong*)b); 
}

static gboolean
remove_each_object (gpointer key, gpointer value, gpointer data)
{
	seahorse_context_remove_object (NULL, value);
	return TRUE;
}

static void 
on_find_objects(GP11Session *session, GAsyncResult *result, SeahorsePkcs11Refresher *self)
{
	GList *objects, *l;
	GError *err = NULL;
	gulong handle;
	
	g_assert (SEAHORSE_IS_PKCS11_REFRESHER (self));
	
	objects = gp11_session_find_objects_finish (session, result, &err);
	if (err != NULL) {
		seahorse_pkcs11_mark_complete (SEAHORSE_OPERATION (self), err);
		return;
	}

	/* Remove all objects that were found, from the check table */
	for (l = objects; l; l = g_list_next (l)) {
		seahorse_pkcs11_source_receive_object (self->source, l->data);
		handle = gp11_object_get_handle (l->data);
		g_hash_table_remove (self->checks, &handle);
	}

	/* Remove everything not found from the context */
	g_hash_table_foreach_remove (self->checks, remove_each_object, NULL);

	seahorse_pkcs11_mark_complete (SEAHORSE_OPERATION (self), NULL);
}

static void 
on_open_session(GP11Slot *slot, GAsyncResult *result, SeahorsePkcs11Refresher *self) 
{
	GError *err = NULL;
	GP11Attributes *attrs;
	
	g_return_if_fail (SEAHORSE_IS_PKCS11_REFRESHER (self));
	
	self->session = gp11_slot_open_session_finish (slot, result, &err);
	if (!self->session) {
		seahorse_pkcs11_mark_complete (SEAHORSE_OPERATION (self), err);
		return;
	}
	
	/* Step 2. Load all the objects that we want */
	attrs = gp11_attributes_new ();
	gp11_attributes_add_boolean (attrs, CKA_TOKEN, TRUE);
	gp11_attributes_add_ulong (attrs, CKA_CLASS, CKO_CERTIFICATE);
	gp11_session_find_objects_async (self->session, attrs, self->cancellable, 
	                                 (GAsyncReadyCallback)on_find_objects, self);
	gp11_attributes_unref (attrs);
}

static void
seahorse_pkcs11_refresher_cancel (SeahorseOperation *operation) 
{
	SeahorsePkcs11Refresher *self = SEAHORSE_PKCS11_REFRESHER (operation);
	g_return_if_fail (SEAHORSE_IS_PKCS11_REFRESHER (self));
	g_cancellable_cancel (self->cancellable);
}

static GObject* 
seahorse_pkcs11_refresher_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	SeahorsePkcs11Refresher *self = SEAHORSE_PKCS11_REFRESHER (G_OBJECT_CLASS (seahorse_pkcs11_refresher_parent_class)->constructor(type, n_props, props));
	GP11Slot *slot;
	GList *objects, *l;
	gulong handle;

	g_return_val_if_fail (self, NULL);	
	g_return_val_if_fail (self->source, NULL);	

	objects = seahorse_context_get_objects (NULL, SEAHORSE_SOURCE (self->source));
	for (l = objects; l; l = g_list_next (l)) {
		if (g_object_class_find_property (G_OBJECT_GET_CLASS (l->data), "pkcs11-handle")) {
			g_object_get (l->data, "pkcs11-handle", &handle, NULL);
			g_hash_table_insert (self->checks, g_memdup (&handle, sizeof (handle)), g_object_ref (l->data));
		}
		
	}

	g_list_free (objects);

	/* Step 1. Load the session */
	slot = seahorse_pkcs11_source_get_slot (self->source);
	gp11_slot_open_session_async (slot, CKF_RW_SESSION, NULL, NULL, self->cancellable,
	                              (GAsyncReadyCallback)on_open_session, self);
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	
	return G_OBJECT (self);
}

static void
seahorse_pkcs11_refresher_init (SeahorsePkcs11Refresher *self)
{
	self->cancellable = g_cancellable_new ();
	self->checks = g_hash_table_new_full (ulong_hash, ulong_equal, g_free, g_object_unref);
}

static void
seahorse_pkcs11_refresher_finalize (GObject *obj)
{
	SeahorsePkcs11Refresher *self = SEAHORSE_PKCS11_REFRESHER (obj);
	
	if (self->cancellable)
		g_object_unref (self->cancellable);
	self->cancellable = NULL;
	
	if (self->source)
		g_object_unref (self->source);
	self->source = NULL;

	if (self->session)
		g_object_unref (self->session);
	self->session = NULL;

	g_hash_table_destroy (self->checks);

	G_OBJECT_CLASS (seahorse_pkcs11_refresher_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_refresher_set_property (GObject *obj, guint prop_id, const GValue *value, 
                                      GParamSpec *pspec)
{
	SeahorsePkcs11Refresher *self = SEAHORSE_PKCS11_REFRESHER (obj);
	
	switch (prop_id) {
	case PROP_SOURCE:
		g_return_if_fail (!self->source);
		self->source = g_value_get_object (value);
		g_return_if_fail (self->source);
		g_object_ref (self->source);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_refresher_get_property (GObject *obj, guint prop_id, GValue *value, 
                                      GParamSpec *pspec)
{
	SeahorsePkcs11Refresher *self = SEAHORSE_PKCS11_REFRESHER (obj);
	
	switch (prop_id) {
	case PROP_SOURCE:
		g_value_set_object (value, self->source);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_refresher_class_init (SeahorsePkcs11RefresherClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseOperationClass *operation_class = SEAHORSE_OPERATION_CLASS (klass);
	
	seahorse_pkcs11_refresher_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructor = seahorse_pkcs11_refresher_constructor;
	gobject_class->finalize = seahorse_pkcs11_refresher_finalize;
	gobject_class->set_property = seahorse_pkcs11_refresher_set_property;
	gobject_class->get_property = seahorse_pkcs11_refresher_get_property;
	
	operation_class->cancel = seahorse_pkcs11_refresher_cancel;
	
	g_object_class_install_property (gobject_class, PROP_SOURCE,
	           g_param_spec_object ("source", "Source", "Source", 
	                                SEAHORSE_TYPE_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
}

SeahorseOperation*
seahorse_pkcs11_refresher_new (SeahorsePkcs11Source *source)
{
	return g_object_new (SEAHORSE_TYPE_PKCS11_REFRESHER, "source", source, NULL);
}


/* -----------------------------------------------------------------------------
 * DELETER OPERATION
 */

#define SEAHORSE_TYPE_PKCS11_DELETER               (seahorse_pkcs11_deleter_get_type ())
#define SEAHORSE_PKCS11_DELETER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_DELETER, SeahorsePkcs11Deleter))
#define SEAHORSE_PKCS11_DELETER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_DELETER, SeahorsePkcs11DeleterClass))
#define SEAHORSE_IS_PKCS11_DELETER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_DELETER))
#define SEAHORSE_IS_PKCS11_DELETER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_DELETER))
#define SEAHORSE_PKCS11_DELETER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_DELETER, SeahorsePkcs11DeleterClass))

typedef struct _SeahorsePkcs11Deleter SeahorsePkcs11Deleter;
typedef struct _SeahorsePkcs11DeleterClass SeahorsePkcs11DeleterClass;
    
struct _SeahorsePkcs11Deleter {
	SeahorseOperation parent;
	SeahorsePkcs11Object *object;
	GCancellable *cancellable;
};

struct _SeahorsePkcs11DeleterClass {
	SeahorseOperationClass parent_class;
};

enum {
	PROP_D0,
	PROP_OBJECT
};

G_DEFINE_TYPE (SeahorsePkcs11Deleter, seahorse_pkcs11_deleter, SEAHORSE_TYPE_OPERATION);

static void 
on_deleted (GP11Object *object, GAsyncResult *result, SeahorsePkcs11Deleter *self) 
{
	GError *err = NULL;
	
	g_return_if_fail (SEAHORSE_IS_PKCS11_DELETER (self));
	
	if (!gp11_object_destroy_finish (object, result, &err)) {

		/* Ignore objects that have gone away */
		if (err->code != CKR_OBJECT_HANDLE_INVALID) { 
			seahorse_pkcs11_mark_complete (SEAHORSE_OPERATION (self), err);
			return;
		}
		
		g_error_free (err);
	}
	
	seahorse_context_remove_object (NULL, SEAHORSE_OBJECT (self->object));
	seahorse_pkcs11_mark_complete (SEAHORSE_OPERATION (self), NULL);
}
			
static void
seahorse_pkcs11_deleter_cancel (SeahorseOperation *operation) 
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (operation);
	g_return_if_fail (SEAHORSE_IS_PKCS11_DELETER (self));
	g_cancellable_cancel (self->cancellable);
}

static GObject* 
seahorse_pkcs11_deleter_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (G_OBJECT_CLASS (seahorse_pkcs11_deleter_parent_class)->constructor(type, n_props, props));
	
	g_return_val_if_fail (self, NULL);
	g_return_val_if_fail (self->object, NULL);

	/* Start the delete */
	gp11_object_destroy_async (seahorse_pkcs11_object_get_pkcs11_object (self->object),
	                           self->cancellable, (GAsyncReadyCallback)on_deleted, self);
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	
	return G_OBJECT (self);
}

static void
seahorse_pkcs11_deleter_init (SeahorsePkcs11Deleter *self)
{
	self->cancellable = g_cancellable_new ();
}

static void
seahorse_pkcs11_deleter_finalize (GObject *obj)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (obj);
	
	if (self->cancellable)
		g_object_unref (self->cancellable);
	self->cancellable = NULL;
	
	if (self->object)
		g_object_unref (self->object);
	self->object = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_deleter_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_deleter_set_property (GObject *obj, guint prop_id, const GValue *value, 
                                      GParamSpec *pspec)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (obj);
	
	switch (prop_id) {
	case PROP_OBJECT:
		g_return_if_fail (!self->object);
		self->object = g_value_get_object (value);
		g_return_if_fail (self->object);
		g_object_ref (self->object);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_deleter_get_property (GObject *obj, guint prop_id, GValue *value, 
                                      GParamSpec *pspec)
{
	SeahorsePkcs11Deleter *self = SEAHORSE_PKCS11_DELETER (obj);
	
	switch (prop_id) {
	case PROP_OBJECT:
		g_value_set_object (value, self->object);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_deleter_class_init (SeahorsePkcs11DeleterClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseOperationClass *operation_class = SEAHORSE_OPERATION_CLASS (klass);
	
	seahorse_pkcs11_deleter_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructor = seahorse_pkcs11_deleter_constructor;
	gobject_class->finalize = seahorse_pkcs11_deleter_finalize;
	gobject_class->set_property = seahorse_pkcs11_deleter_set_property;
	gobject_class->get_property = seahorse_pkcs11_deleter_get_property;
	
	operation_class->cancel = seahorse_pkcs11_deleter_cancel;
	
	g_object_class_install_property (gobject_class, PROP_OBJECT,
	           g_param_spec_object ("object", "Object", "Deleting Object", 
	                                SEAHORSE_PKCS11_TYPE_OBJECT, 
	                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
}

SeahorseOperation*
seahorse_pkcs11_deleter_new (SeahorsePkcs11Object *object)
{
	return g_object_new (SEAHORSE_TYPE_PKCS11_DELETER, "object", object, NULL);
}
