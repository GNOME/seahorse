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

#include "seahorse-ssh-backend.h"
#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-source.h"

#include "seahorse-backend.h"

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_NAME,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_ACTIONS
};

static SeahorseSshBackend *ssh_backend = NULL;

struct _SeahorseSshBackend {
	GObject parent;
	SeahorseSSHSource *dot_ssh;
};

struct _SeahorseSshBackendClass {
	GObjectClass parent_class;
};

static void         seahorse_ssh_backend_iface            (SeahorseBackendIface *iface);

static void         seahorse_ssh_backend_collection_init  (GcrCollectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (SeahorseSshBackend, seahorse_ssh_backend, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GCR_TYPE_COLLECTION, seahorse_ssh_backend_collection_init);
                         G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_BACKEND, seahorse_ssh_backend_iface);
);

static void
seahorse_ssh_backend_init (SeahorseSshBackend *self)
{
	g_return_if_fail (ssh_backend == NULL);
	ssh_backend = self;

	seahorse_ssh_generate_register ();
}

static void
seahorse_ssh_backend_constructed (GObject *obj)
{
	SeahorseSshBackend *self = SEAHORSE_SSH_BACKEND (obj);

	G_OBJECT_CLASS (seahorse_ssh_backend_parent_class)->constructed (obj);

	self->dot_ssh = seahorse_ssh_source_new ();
	seahorse_place_load_async (SEAHORSE_PLACE (self->dot_ssh), NULL, NULL, NULL);
}

static void
seahorse_ssh_backend_get_property (GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, SEAHORSE_SSH_NAME);
		break;
	case PROP_LABEL:
		g_value_set_string (value, _("Secure Shell"));
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, _("Keys used to connect securely to other computers"));
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_ssh_backend_finalize (GObject *obj)
{
	SeahorseSshBackend *self = SEAHORSE_SSH_BACKEND (obj);

	g_clear_object (&self->dot_ssh);
	g_return_if_fail (ssh_backend == self);
	ssh_backend = NULL;

	G_OBJECT_CLASS (seahorse_ssh_backend_parent_class)->finalize (obj);
}

static void
seahorse_ssh_backend_class_init (SeahorseSshBackendClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructed = seahorse_ssh_backend_constructed;
	gobject_class->finalize = seahorse_ssh_backend_finalize;
	gobject_class->get_property = seahorse_ssh_backend_get_property;

	g_object_class_override_property (gobject_class, PROP_NAME, "name");
	g_object_class_override_property (gobject_class, PROP_LABEL, "label");
	g_object_class_override_property (gobject_class, PROP_DESCRIPTION, "description");
	g_object_class_override_property (gobject_class, PROP_ACTIONS, "actions");
}

static guint
seahorse_ssh_backend_get_length (GcrCollection *collection)
{
	return 1;
}

static GList *
seahorse_ssh_backend_get_objects (GcrCollection *collection)
{
	SeahorseSshBackend *self = SEAHORSE_SSH_BACKEND (collection);
	return g_list_append (NULL, self->dot_ssh);
}

static gboolean
seahorse_ssh_backend_contains (GcrCollection *collection,
                               GObject *object)
{
	SeahorseSshBackend *self = SEAHORSE_SSH_BACKEND (collection);
	return G_OBJECT (self->dot_ssh) == object;
}

static void
seahorse_ssh_backend_collection_init (GcrCollectionIface *iface)
{
	iface->contains = seahorse_ssh_backend_contains;
	iface->get_length = seahorse_ssh_backend_get_length;
	iface->get_objects = seahorse_ssh_backend_get_objects;
}

static SeahorsePlace *
seahorse_ssh_backend_lookup_place (SeahorseBackend *backend,
                                   const gchar *uri)
{
	SeahorseSshBackend *self = SEAHORSE_SSH_BACKEND (backend);
	gchar *our_uri = NULL;
	gboolean match;

	if (self->dot_ssh) {
		g_object_get (self->dot_ssh, "uri", &our_uri, NULL);
		match = (our_uri && g_str_equal (our_uri, uri));
		g_free (our_uri);
		if (match)
			return SEAHORSE_PLACE (self->dot_ssh);
	}

	return NULL;
}

static void
seahorse_ssh_backend_iface (SeahorseBackendIface *iface)
{
	iface->lookup_place = seahorse_ssh_backend_lookup_place;
}

void
seahorse_ssh_backend_initialize (void)
{
	SeahorseSshBackend *self;

	g_return_if_fail (ssh_backend == NULL);
	self = g_object_new (SEAHORSE_TYPE_SSH_BACKEND, NULL);

	seahorse_backend_register (SEAHORSE_BACKEND (self));
	g_object_unref (self);

	g_return_if_fail (ssh_backend != NULL);
}

SeahorseSshBackend *
seahorse_ssh_backend_get (void)
{
	g_return_val_if_fail (ssh_backend, NULL);
	return ssh_backend;
}

SeahorseSSHSource *
seahorse_ssh_backend_get_dot_ssh (SeahorseSshBackend *self)
{
	self = self ? self : seahorse_ssh_backend_get ();
	g_return_val_if_fail (SEAHORSE_IS_SSH_BACKEND (self), NULL);
	g_return_val_if_fail (self->dot_ssh, NULL);
	return self->dot_ssh;
}
