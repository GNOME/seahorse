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
#include "seahorse-pkcs11-token.h"

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
	GList *blacklist;
};

struct _SeahorsePkcs11BackendClass {
	GObjectClass parent_class;
};

static const char *token_blacklist[] = {
	"pkcs11:manufacturer=Gnome%20Keyring;serial=1:SSH:HOME",
	"pkcs11:manufacturer=Gnome%20Keyring;serial=1:SECRET:MAIN",
	NULL
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
	GError *error = NULL;
	GckUriData *uri;
	guint i;

	g_return_if_fail (pkcs11_backend == NULL);
	pkcs11_backend = self;

	/* Let these classes register themselves, when the backend is created */
	g_type_class_unref (g_type_class_ref (SEAHORSE_PKCS11_TYPE_COMMANDS));

	for (i = 0; token_blacklist[i] != NULL; i++) {
		uri = gck_uri_parse (token_blacklist[i], GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE, &error);
		if (uri == NULL) {
			g_warning ("couldn't parse pkcs11 blacklist uri: %s", error->message);
			g_clear_error (&error);
		}
		self->blacklist = g_list_prepend (self->blacklist, uri);
	}
}

static gboolean
is_token_usable (SeahorsePkcs11Backend *self,
                 GckSlot *slot,
                 GckTokenInfo *token)
{
	GList *l;

	if (token->flags & CKF_WRITE_PROTECTED) {
		/* _gcr_debug ("token is not importable: %s: write protected", token->label); */
		return FALSE;
	}
	if (!(token->flags & CKF_TOKEN_INITIALIZED)) {
		/* _gcr_debug ("token is not importable: %s: not initialized", token->label); */
		return FALSE;
	}
	if ((token->flags & CKF_LOGIN_REQUIRED) &&
	    !(token->flags & CKF_USER_PIN_INITIALIZED)) {
		/* _gcr_debug ("token is not importable: %s: user pin not initialized", token->label); */
		return FALSE;
	}

	for (l = self->blacklist; l != NULL; l = g_list_next (l)) {
		if (gck_slot_match (slot, l->data))
			return FALSE;
	}

	return TRUE;
}


static void
on_initialized_registered (GObject *unused,
                           GAsyncResult *result,
                           gpointer user_data)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (user_data);
	SeahorseSource *source;
	GList *slots, *s;
	GList *modules, *m;
	GError *error = NULL;
	GckTokenInfo *token;

	modules = gck_modules_initialize_registered_finish (result, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	for (m = modules; m != NULL; m = g_list_next (m)) {
		slots = gck_module_get_slots (m->data, TRUE);
		for (s = slots; s; s = g_list_next (s)) {
			token = gck_slot_get_token_info (s->data);
			if (is_token_usable (self, s->data, token)) {
				source = SEAHORSE_SOURCE (seahorse_pkcs11_token_new (s->data));
				self->slots = g_list_append (self->slots, source);
				gcr_collection_emit_added (GCR_COLLECTION (self), G_OBJECT (source));
			}
			gck_token_info_free (token);
		}

		/* These will have been refed by the source above */
		gck_list_unref_free (slots);
	}

	gck_list_unref_free (modules);
	g_object_unref (self);
}

static void
seahorse_pkcs11_backend_constructed (GObject *obj)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (obj);

	G_OBJECT_CLASS (seahorse_pkcs11_backend_parent_class)->constructed (obj);

	gck_modules_initialize_registered_async (NULL, on_initialized_registered,
	                                         g_object_ref (self));
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

	g_list_free_full (self->blacklist, (GDestroyNotify)gck_uri_data_free);
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
