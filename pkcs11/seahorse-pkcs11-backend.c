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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "seahorse-common.h"

#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-backend.h"
#include "seahorse-pkcs11-generate.h"

#include "seahorse-util.h"

#include <gcr/gcr-base.h>

#include <gck/gck.h>

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_NAME,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_ACTIONS
};

void  seahorse_pkcs11_backend_initialize (void);

static SeahorsePkcs11Backend *pkcs11_backend = NULL;

struct _SeahorsePkcs11Backend {
	GObject parent;
	GList *tokens;
	GList *blacklist;
};

struct _SeahorsePkcs11BackendClass {
	GObjectClass parent_class;
};

static const char *token_blacklist[] = {
	"pkcs11:manufacturer=Gnome%20Keyring;serial=1:SSH:HOME",
	"pkcs11:manufacturer=Gnome%20Keyring;serial=1:SECRET:MAIN",
	"pkcs11:manufacturer=Mozilla%20Foundation;token=NSS%20Generic%20Crypto%20Services",
	NULL
};

static void         seahorse_pkcs11_backend_iface            (SeahorseBackendIface *iface);

static void         seahorse_pkcs11_backend_collection_init  (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorsePkcs11Backend, seahorse_pkcs11_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_pkcs11_backend_collection_init)
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_BACKEND, seahorse_pkcs11_backend_iface);
);

static void
seahorse_pkcs11_backend_init (SeahorsePkcs11Backend *self)
{
	GError *error = NULL;
	GckUriData *uri;
	guint i;

	g_return_if_fail (pkcs11_backend == NULL);
	pkcs11_backend = self;

	for (i = 0; token_blacklist[i] != NULL; i++) {
		uri = gck_uri_parse (token_blacklist[i], GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE, &error);
		if (uri == NULL) {
			g_warning ("couldn't parse pkcs11 blacklist uri: %s", error->message);
			g_clear_error (&error);
		}
		self->blacklist = g_list_prepend (self->blacklist, uri);
	}

	seahorse_pkcs11_generate_register ();
}

static gboolean
is_token_usable (SeahorsePkcs11Backend *self,
                 GckSlot *slot,
                 GckTokenInfo *token)
{
	GList *l;

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
	SeahorsePlace *place;
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
			if (token == NULL)
				continue;
			if (is_token_usable (self, s->data, token)) {
				place = SEAHORSE_PLACE (seahorse_pkcs11_token_new (s->data));
				self->tokens = g_list_append (self->tokens, place);
				gcr_collection_emit_added (GCR_COLLECTION (self), G_OBJECT (place));
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

static const gchar *
seahorse_pkcs11_backend_get_name (SeahorseBackend *backend)
{
	return SEAHORSE_PKCS11_NAME;
}

static const gchar *
seahorse_pkcs11_backend_get_label (SeahorseBackend *backend)
{
	return _("Certificates");
}

static const gchar *
seahorse_pkcs11_backend_get_description (SeahorseBackend *backend)
{
	return _("X.509 certificates and related keys");
}

static GtkActionGroup *
seahorse_pkcs11_backend_get_actions (SeahorseBackend *backend)
{
	return NULL;
}

static void
seahorse_pkcs11_backend_get_property (GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	SeahorseBackend *backend = SEAHORSE_BACKEND (obj);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, seahorse_pkcs11_backend_get_name (backend));
		break;
	case PROP_LABEL:
		g_value_set_string (value, seahorse_pkcs11_backend_get_label (backend));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, seahorse_pkcs11_backend_get_description (backend));
		break;
	case PROP_ACTIONS:
		g_value_take_object (value, seahorse_pkcs11_backend_get_actions (backend));
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

	g_list_free_full (self->tokens, g_object_unref);
	self->tokens = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_backend_parent_class)->dispose (obj);
}

static void
seahorse_pkcs11_backend_finalize (GObject *obj)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (obj);

	g_list_free_full (self->blacklist, (GDestroyNotify)gck_uri_data_free);
	g_assert (self->tokens == NULL);
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
	g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
}

static guint
seahorse_pkcs11_backend_get_length (GcrCollection *collection)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (collection);
	return g_list_length (self->tokens);
}

static GList *
seahorse_pkcs11_backend_get_objects (GcrCollection *collection)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (collection);
	return g_list_copy (self->tokens);
}

static gboolean
seahorse_pkcs11_backend_contains (GcrCollection *collection,
                                  GObject *object)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (collection);
	return g_list_find (self->tokens, object) != NULL;
}

static void
seahorse_pkcs11_backend_collection_init (GcrCollectionIface *iface)
{
	iface->contains = seahorse_pkcs11_backend_contains;
	iface->get_length = seahorse_pkcs11_backend_get_length;
	iface->get_objects = seahorse_pkcs11_backend_get_objects;
}

static SeahorsePlace *
seahorse_pkcs11_backend_lookup_place (SeahorseBackend *backend,
                                      const gchar *uri)
{
	SeahorsePkcs11Backend *self = SEAHORSE_PKCS11_BACKEND (backend);
	GckUriData *uri_data;
	GList *l;

	if (!g_str_has_prefix (uri, "pkcs11:"))
		return NULL;

	uri_data = gck_uri_parse (uri, GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE, NULL);
	if (uri_data == NULL)
		return NULL;

	for (l = self->tokens; l != NULL; l = g_list_next (l)) {
		if (gck_slot_match (seahorse_pkcs11_token_get_slot (l->data), uri_data))
			break;
	}

	gck_uri_data_free (uri_data);
	return l != NULL ? l->data : NULL;
}

static void
seahorse_pkcs11_backend_iface (SeahorseBackendIface *iface)
{
	iface->lookup_place = seahorse_pkcs11_backend_lookup_place;
	iface->get_actions = seahorse_pkcs11_backend_get_actions;
	iface->get_description = seahorse_pkcs11_backend_get_description;
	iface->get_label = seahorse_pkcs11_backend_get_label;
	iface->get_name = seahorse_pkcs11_backend_get_name;
}

void
seahorse_pkcs11_backend_initialize (void)
{
	SeahorsePkcs11Backend *self;

	g_return_if_fail (pkcs11_backend == NULL);
	self = g_object_new (SEAHORSE_TYPE_PKCS11_BACKEND, NULL);

	seahorse_backend_register (SEAHORSE_BACKEND (self));
	g_object_unref (self);

	g_return_if_fail (pkcs11_backend != NULL);
}

SeahorsePkcs11Backend *
seahorse_pkcs11_backend_get (void)
{
	g_return_val_if_fail (pkcs11_backend, NULL);
	return pkcs11_backend;
}

static gboolean
on_filter_writable (GObject *object,
                    gpointer user_data)
{
	SeahorsePkcs11Token *token = SEAHORSE_PKCS11_TOKEN (object);
	guint mechanism = GPOINTER_TO_UINT (user_data);
	GckTokenInfo *info;

	info = seahorse_pkcs11_token_get_info (token);
	g_return_val_if_fail (info != NULL, FALSE);

	if (info->flags & CKF_WRITE_PROTECTED)
		return FALSE;

	if (mechanism != G_MAXUINT) {
		if (!seahorse_pkcs11_token_has_mechanism (token, (gulong)mechanism))
			return FALSE;
	}

	return TRUE;
}

GcrCollection *
seahorse_pkcs11_backend_get_writable_tokens (SeahorsePkcs11Backend *self,
                                             gulong with_mechanism)
{
	gpointer mechanism;

	self = self ? self : seahorse_pkcs11_backend_get ();
	g_return_val_if_fail (SEAHORSE_IS_PKCS11_BACKEND (self), NULL);

	if (with_mechanism == GCK_INVALID)
		mechanism = GUINT_TO_POINTER (G_MAXUINT);
	else
		mechanism = GUINT_TO_POINTER (with_mechanism);

	return gcr_filter_collection_new_with_callback (GCR_COLLECTION (self),
	                                                on_filter_writable,
	                                                mechanism, NULL);
}
