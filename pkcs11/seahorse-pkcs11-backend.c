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

#include "seahorse-pkcs11-backend.h"
#include "seahorse-pkcs11-commands.h"
#include "seahorse-pkcs11-source.h"

#include "seahorse-backend.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

#include <gck/gck.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_NAME,
	PROP_LABEL,
	PROP_DESCRIPTION
};

static SeahorsePkcs11Backend *pkcs11_backend = NULL;

struct _SeahorsePkcs11Backend {
	GObject parent;
	GList *slots;
};

struct _SeahorsePkcs11BackendClass {
	GObjectClass parent_class;
};

static void         seahorse_pkcs11_backend_iface_init       (SeahorseBackendIface *iface);

static void         seahorse_pkcs11_backend_collection_init  (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorsePkcs11Backend, seahorse_pkcs11_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_pkcs11_backend_collection_init)
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_BACKEND, seahorse_pkcs11_backend_iface_init);
);

static void
seahorse_pkcs11_backend_init (SeahorsePkcs11Backend *self)
{
	g_return_if_fail (pkcs11_backend == NULL);
	pkcs11_backend = self;

	/* Let these classes register themselves, when the backend is created */
	g_type_class_unref (g_type_class_ref (SEAHORSE_PKCS11_TYPE_COMMANDS));
}

static void
seahorse_pkcs11_backend_constructed (GObject *obj)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (obj);
	SeahorseSource *source;
	GList *slots, *s;
	GList *modules, *m;
	GError *error = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_backend_parent_class)->constructed (obj);

	modules = gck_modules_initialize_registered (NULL, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	for (m = modules; m != NULL; m = g_list_next (m)) {
		slots = gck_module_get_slots (m->data, FALSE);
		for (s = slots; s; s = g_list_next (s)) {
			source = SEAHORSE_SOURCE (seahorse_pkcs11_source_new (s->data));
			self->slots = g_list_append (self->slots, source);
		}

		/* These will have been refed by the source above */
		gck_list_unref_free (slots);
	}

	gck_list_unref_free (modules);

}

static void
seahorse_pkcs11_backend_get_property (GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, SEAHORSE_PKCS11_NAME);
		break;
	case PROP_LABEL:
		g_value_set_string (value, _("Certificates"));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, _("X.509 certificates and related keys"));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_backend_dispose (GObject *obj)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (obj);

	g_list_free_full (self->slots, g_object_unref);
	self->slots = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_backend_parent_class)->dispose (obj);
}

static void
seahorse_pkcs11_backend_finalize (GObject *obj)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (obj);

	g_assert (self->slots == NULL);
	g_return_if_fail (pkcs11_backend == self);
	pkcs11_backend = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_backend_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_backend_class_init (SeahorsePkcs11BackendClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_pkcs11_backend_constructed;
	gobject_class->dispose = seahorse_pkcs11_backend_dispose;
	gobject_class->finalize = seahorse_pkcs11_backend_finalize;
	gobject_class->get_property = seahorse_pkcs11_backend_get_property;

	g_object_class_override_property (gobject_class, PROP_NAME, "name");
	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
}

static guint
seahorse_pkcs11_backend_get_length (GcrCollection *collection)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (collection);
	return g_list_length (self->slots);
}

static GList *
seahorse_pkcs11_backend_get_objects (GcrCollection *collection)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (collection);
	return g_list_copy (self->slots);
}

static gboolean
seahorse_pkcs11_backend_contains (GcrCollection *collection,
                                  GObject *object)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (collection);
	return g_list_find (self->slots, object) != NULL;
}

static void
seahorse_pkcs11_backend_collection_init (GcrCollectionIface *iface)
{
	iface->contains = seahorse_pkcs11_backend_contains;
	iface->get_length = seahorse_pkcs11_backend_get_length;
	iface->get_objects = seahorse_pkcs11_backend_get_objects;
}


static void
seahorse_pkcs11_backend_iface_init (SeahorseBackendIface *iface)
{

}

void
seahorse_pkcs11_backend_initialize (void)
{
	SeahorsePkcs11Backend *self;

	g_return_if_fail (pkcs11_backend == NULL);
	self = g_object_new (SEAHORSE_TYPE_PKCS11_BACKEND, NULL);

	seahorse_registry_register_object (NULL, G_OBJECT (self), "backend", "pkcs11", NULL);
	g_object_unref (self);

	g_return_if_fail (pkcs11_backend != NULL);
}

SeahorsePkcs11Backend *
seahorse_pkcs11_backend_get (void)
{
	g_return_val_if_fail (pkcs11_backend, NULL);
	return pkcs11_backend;
}
