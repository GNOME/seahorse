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
#include "seahorse-pkcs11-token.h"

#include "seahorse-registry.h"

enum {
	PROP_0,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_ICON,
	PROP_SLOT,
	PROP_FLAGS
};

struct _SeahorsePkcs11TokenPrivate {
	GckSlot *slot;
	GHashTable *objects;
};

static void          seahorse_pkcs11_token_source_iface  (SeahorseSourceIface *iface);

static void          seahorse_pkcs11_collection_iface    (GcrCollectionIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorsePkcs11Token, seahorse_pkcs11_token, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_pkcs11_collection_iface);
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_pkcs11_token_source_iface);
);

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_pkcs11_token_init (SeahorsePkcs11Token *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PKCS11_TOKEN, SeahorsePkcs11TokenPrivate));
	self->pv->objects = g_hash_table_new_full (seahorse_pkcs11_ulong_hash,
	                                           seahorse_pkcs11_ulong_equal,
	                                           g_free, g_object_unref);
}

static void
seahorse_pkcs11_token_constructed (GObject *obj)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (obj);

	G_OBJECT_CLASS (seahorse_pkcs11_token_parent_class)->constructed (obj);

	seahorse_pkcs11_refresh_async (self, NULL, NULL, NULL);
}

static void
seahorse_pkcs11_token_get_property (GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (object);
	GckTokenInfo *token;

	switch (prop_id) {
	case PROP_LABEL:
		token = gck_slot_get_token_info (self->pv->slot);
		if (token == NULL)
			g_value_set_string (value, _("Unknown"));
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
	}
}

static void
seahorse_pkcs11_token_set_property (GObject *object,
                                    guint prop_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (object);

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
seahorse_pkcs11_token_dispose (GObject *obj)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (obj);

	/* The keyring object */
	if (self->pv->slot)
		g_object_unref (self->pv->slot);
	self->pv->slot = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_token_parent_class)->dispose (obj);
}

static void
seahorse_pkcs11_token_finalize (GObject *obj)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (obj);

	g_hash_table_destroy (self->pv->objects);
	g_assert (self->pv->slot == NULL);

	G_OBJECT_CLASS (seahorse_pkcs11_token_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_token_class_init (SeahorsePkcs11TokenClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (SeahorsePkcs11TokenPrivate));

	gobject_class->constructed = seahorse_pkcs11_token_constructed;
	gobject_class->dispose = seahorse_pkcs11_token_dispose;
	gobject_class->finalize = seahorse_pkcs11_token_finalize;
	gobject_class->set_property = seahorse_pkcs11_token_set_property;
	gobject_class->get_property = seahorse_pkcs11_token_get_property;

	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
	g_object_class_override_property (gobject_class, PROP_ICON, "icon");

	g_object_class_install_property (gobject_class, PROP_SLOT,
	         g_param_spec_object ("slot", "Slot", "Pkcs#11 SLOT",
	                              GCK_TYPE_SLOT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	         g_param_spec_uint ("flags", "Flags", "Object Token flags.",
	                            0, G_MAXUINT, 0, G_PARAM_READABLE));
}

static void
seahorse_pkcs11_token_source_iface (SeahorseSourceIface *iface)
{

}

static guint
seahorse_pkcs11_token_get_length (GcrCollection *collection)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (collection);
	return g_hash_table_size (self->pv->objects);
}

static GList *
seahorse_pkcs11_token_get_objects (GcrCollection *collection)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (collection);
	return g_hash_table_get_values (self->pv->objects);
}

static gboolean
seahorse_pkcs11_token_contains (GcrCollection *collection,
                                GObject *object)
{
	SeahorsePkcs11Token *self = SEAHORSE_PKCS11_TOKEN (collection);
	gulong handle;

	if (!SEAHORSE_PKCS11_IS_OBJECT (object))
		return FALSE;

	handle = seahorse_pkcs11_object_get_pkcs11_handle (SEAHORSE_PKCS11_OBJECT (object));
	return g_hash_table_lookup (self->pv->objects, &handle) == object;
}

static void
seahorse_pkcs11_collection_iface (GcrCollectionIface *iface)
{
	iface->get_length = seahorse_pkcs11_token_get_length;
	iface->get_objects = seahorse_pkcs11_token_get_objects;
	iface->contains = seahorse_pkcs11_token_contains;
}

/* --------------------------------------------------------------------------
 * PUBLIC
 */

SeahorsePkcs11Token *
seahorse_pkcs11_token_new (GckSlot *slot)
{
	return g_object_new (SEAHORSE_TYPE_PKCS11_TOKEN, "slot", slot, NULL);
}

GckSlot *
seahorse_pkcs11_token_get_slot (SeahorsePkcs11Token *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PKCS11_TOKEN (self), NULL);
	return self->pv->slot;
}

void
seahorse_pkcs11_token_receive_object (SeahorsePkcs11Token *self,
                                      GckObject *obj)
{
	SeahorsePkcs11Certificate *cert;
	SeahorseObject *prev;
	gulong handle;

	g_return_if_fail (SEAHORSE_IS_PKCS11_TOKEN (self));

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
seahorse_pkcs11_token_remove_object (SeahorsePkcs11Token *self,
                                     SeahorsePkcs11Object *object)
{
	gulong handle;

	g_return_if_fail (SEAHORSE_IS_PKCS11_TOKEN (self));
	g_return_if_fail (SEAHORSE_PKCS11_IS_OBJECT (object));

	g_object_ref (object);

	handle = seahorse_pkcs11_object_get_pkcs11_handle (object);
	g_return_if_fail (g_hash_table_lookup (self->pv->objects, &handle) == object);

	g_hash_table_remove (self->pv->objects, &handle);
	gcr_collection_emit_removed (GCR_COLLECTION (self), G_OBJECT (object));

	g_object_unref (object);
}
