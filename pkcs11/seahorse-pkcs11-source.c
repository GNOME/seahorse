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

#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-certificate.h"
#include "seahorse-pkcs11-object.h"
#include "seahorse-pkcs11-operations.h"
#include "seahorse-pkcs11-source.h"

#include "common/seahorse-registry.h"

enum {
    PROP_0,
    PROP_SLOT,
    PROP_KEY_TYPE,
    PROP_FLAGS,
    PROP_LOCATION
};

struct _SeahorsePkcs11SourcePrivate {
	GP11Slot *slot;    
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorsePkcs11Source, seahorse_pkcs11_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_pkcs11_source_init (SeahorsePkcs11Source *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PKCS11_SOURCE, SeahorsePkcs11SourcePrivate));
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
	case PROP_KEY_TYPE:
		g_value_set_uint (value, SEAHORSE_PKCS11_TYPE);
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, 0);
		break;
	case PROP_LOCATION:
		g_value_set_enum (value, SEAHORSE_LOCATION_LOCAL);
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

static SeahorseOperation*
seahorse_pkcs11_source_load (SeahorseSource *src)
{
	return seahorse_pkcs11_refresher_new (SEAHORSE_PKCS11_SOURCE (src));
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
	                              GP11_TYPE_SLOT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    
	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_uint ("flags", "Flags", "Object Source flags.", 
	                            0, G_MAXUINT, 0, G_PARAM_READABLE));

	g_object_class_override_property (gobject_class, PROP_KEY_TYPE, "key-type");
	g_object_class_override_property (gobject_class, PROP_LOCATION, "location");
    
	seahorse_registry_register_type (NULL, SEAHORSE_TYPE_PKCS11_SOURCE, "source", "local", SEAHORSE_PKCS11_TYPE_STR, NULL);
}

static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load = seahorse_pkcs11_source_load;
}

/* -------------------------------------------------------------------------- 
 * PUBLIC
 */

SeahorsePkcs11Source*
seahorse_pkcs11_source_new (GP11Slot *slot)
{
	return g_object_new (SEAHORSE_TYPE_PKCS11_SOURCE, "slot", slot, NULL);
}   

GP11Slot*
seahorse_pkcs11_source_get_slot (SeahorsePkcs11Source *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PKCS11_SOURCE (self), NULL);
	return self->pv->slot;
}

void
seahorse_pkcs11_source_receive_object (SeahorsePkcs11Source *self, GP11Object *obj)
{
	GQuark id = seahorse_pkcs11_object_cannonical_id (obj);
	SeahorsePkcs11Certificate *cert;
	SeahorseObject *prev;
	
	g_return_if_fail (SEAHORSE_IS_PKCS11_SOURCE (self));
	
	/* TODO: This will need to change once we get other kinds of objects */
	
	prev = seahorse_context_get_object (NULL, SEAHORSE_SOURCE (self), id);
	if (prev) {
		seahorse_object_refresh (prev);
		g_object_unref (obj);
		return;
	}

	cert = seahorse_pkcs11_certificate_new (obj);
	g_object_set (cert, "source", self, NULL);
	seahorse_context_add_object (NULL, SEAHORSE_OBJECT (cert));
}
