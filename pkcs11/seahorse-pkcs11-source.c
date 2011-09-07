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

#include "seahorse-util.h"
#include "seahorse-secure-memory.h"
#include "seahorse-passphrase.h"

#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-certificate.h"
#include "seahorse-pkcs11-helpers.h"
#include "seahorse-pkcs11-object.h"
#include "seahorse-pkcs11-operations.h"
#include "seahorse-pkcs11-source.h"

#include "seahorse-registry.h"

enum {
    PROP_0,
    PROP_SLOT,
    PROP_FLAGS
};

struct _SeahorsePkcs11SourcePrivate {
	GckSlot *slot;
	GHashTable *objects;
};

static void          seahorse_pkcs11_source_iface      (SeahorseSourceIface *iface);

static void          seahorse_pkcs11_collection_iface  (GcrCollectionIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorsePkcs11Source, seahorse_pkcs11_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_pkcs11_source_iface);
                        G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_pkcs11_collection_iface);
);

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_pkcs11_source_init (SeahorsePkcs11Source *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PKCS11_SOURCE, SeahorsePkcs11SourcePrivate));
	self->pv->objects = g_hash_table_new_full (seahorse_pkcs11_ulong_hash,
	                                           seahorse_pkcs11_ulong_equal,
	                                           g_free, g_object_unref);
}

static GObject*  
seahorse_pkcs11_source_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
	GObject* obj = G_OBJECT_CLASS (seahorse_pkcs11_source_parent_class)->constructor (type, n_props, props);
	SeahorsePkcs11Source *self = NULL;
	
	if (obj) {
		self = SEAHORSE_PKCS11_SOURCE (obj);
		g_return_val_if_fail (self->pv->slot, NULL);
	}
	
	return obj;
}

static void 
seahorse_pkcs11_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                       GParamSpec *pspec)
{
	SeahorsePkcs11Source *self = SEAHORSE_PKCS11_SOURCE (object);

	switch (prop_id) {
	case PROP_SLOT:
		g_value_set_object (value, self->pv->slot);
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, 0);
		break;
	}
}

static void 
seahorse_pkcs11_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                     GParamSpec *pspec)
{
	SeahorsePkcs11Source *self = SEAHORSE_PKCS11_SOURCE (object);

	switch (prop_id) {
	case PROP_SLOT:
		g_return_if_fail (!self->pv->slot);
		self->pv->slot = g_value_get_object (value);
		g_return_if_fail (self->pv->slot);
		g_object_ref (self->pv->slot);
		break;
	};
}

static void
seahorse_pkcs11_source_dispose (GObject *obj)
{
	SeahorsePkcs11Source *self = SEAHORSE_PKCS11_SOURCE (obj);
    
	/* The keyring object */
	if (self->pv->slot)
		g_object_unref (self->pv->slot);
	self->pv->slot = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_source_parent_class)->dispose (obj);
}

static void
seahorse_pkcs11_source_finalize (GObject *obj)
{
	SeahorsePkcs11Source *self = SEAHORSE_PKCS11_SOURCE (obj);

	g_hash_table_destroy (self->pv->objects);
	g_assert (self->pv->slot == NULL);
    
	G_OBJECT_CLASS (seahorse_pkcs11_source_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_source_class_init (SeahorsePkcs11SourceClass *klass)
{
	GObjectClass *gobject_class;
    
	seahorse_pkcs11_source_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11SourcePrivate));
	
	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = seahorse_pkcs11_source_constructor;
	gobject_class->dispose = seahorse_pkcs11_source_dispose;
	gobject_class->finalize = seahorse_pkcs11_source_finalize;
	gobject_class->set_property = seahorse_pkcs11_source_set_property;
	gobject_class->get_property = seahorse_pkcs11_source_get_property;
    
	g_object_class_install_property (gobject_class, PROP_SLOT,
	         g_param_spec_object ("slot", "Slot", "Pkcs#11 SLOT",
	                              GCK_TYPE_SLOT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_uint ("flags", "Flags", "Object Source flags.", 
	                            0, G_MAXUINT, 0, G_PARAM_READABLE));

	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_PKCS11_SOURCE, "source", "local", SEAHORSE_PKCS11_TYPE_STR, NULL);
}

static void 
seahorse_pkcs11_source_iface (SeahorseSourceIface *iface)
{

}


static guint
seahorse_pkcs11_source_get_length (GcrCollection *collection)
{
	SeahorsePkcs11Source *self = SEAHORSE_PKCS11_SOURCE (collection);
	return g_hash_table_size (self->pv->objects);
}

static GList *
seahorse_pkcs11_source_get_objects (GcrCollection *collection)
{
	SeahorsePkcs11Source *self = SEAHORSE_PKCS11_SOURCE (collection);
	return g_hash_table_get_values (self->pv->objects);
}

static gboolean
seahorse_pkcs11_source_contains (GcrCollection *collection,
                                 GObject *object)
{
	SeahorsePkcs11Source *self = SEAHORSE_PKCS11_SOURCE (collection);
	gulong handle;

	if (!SEAHORSE_PKCS11_IS_OBJECT (object))
		return FALSE;

	handle = seahorse_pkcs11_object_get_pkcs11_handle (SEAHORSE_PKCS11_OBJECT (object));
	return g_hash_table_lookup (self->pv->objects, &handle) == object;
}

static void
seahorse_pkcs11_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = seahorse_pkcs11_source_get_length;
	iface->get_objects = seahorse_pkcs11_source_get_objects;
	iface->contains = seahorse_pkcs11_source_contains;
}

/* -------------------------------------------------------------------------- 
 * PUBLIC
 */

SeahorsePkcs11Source*
seahorse_pkcs11_source_new (GckSlot *slot)
{
	return g_object_new (SEAHORSE_TYPE_PKCS11_SOURCE, "slot", slot, NULL);
}

GckSlot*
seahorse_pkcs11_source_get_slot (SeahorsePkcs11Source *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PKCS11_SOURCE (self), NULL);
	return self->pv->slot;
}

void
seahorse_pkcs11_source_receive_object (SeahorsePkcs11Source *self,
                                       GckObject *obj)
{
	SeahorsePkcs11Certificate *cert;
	SeahorseObject *prev;
	gulong handle;

	g_return_if_fail (SEAHORSE_IS_PKCS11_SOURCE (self));

	handle = gck_object_get_handle (obj);
	prev = g_hash_table_lookup (self->pv->objects, &handle);
	if (prev != NULL) {
		seahorse_pkcs11_object_refresh (SEAHORSE_PKCS11_OBJECT (prev));
		g_object_unref (obj);
		return;
	}

	cert = seahorse_pkcs11_certificate_new (obj);
	g_object_set (cert, "source", self, NULL);

	g_hash_table_insert (self->pv->objects, g_memdup (&handle, sizeof (handle)), cert);
	gcr_collection_emit_added (GCR_COLLECTION (self), G_OBJECT (cert));
}

void
seahorse_pkcs11_source_remove_object (SeahorsePkcs11Source *self,
                                      SeahorsePkcs11Object *object)
{
	gulong handle;

	g_return_if_fail (SEAHORSE_IS_PKCS11_SOURCE (self));
	g_return_if_fail (SEAHORSE_PKCS11_IS_OBJECT (object));

	g_object_ref (object);

	handle = seahorse_pkcs11_object_get_pkcs11_handle (object);
	g_return_if_fail (g_hash_table_lookup (self->pv->objects, &handle) == object);

	g_hash_table_remove (self->pv->objects, &handle);
	gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (object));

	g_object_unref (object);
}
