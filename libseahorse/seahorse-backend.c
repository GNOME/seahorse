/*
 * Seahorse
 *
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

#include "seahorse-backend.h"
#include "seahorse-registry.h"

#include <gcr/gcr.h>

typedef SeahorseBackendIface SeahorseBackendInterface;

G_DEFINE_INTERFACE (SeahorseBackend, seahorse_backend, GCR_TYPE_COLLECTION);

static void
seahorse_backend_default_init (SeahorseBackendIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		g_object_interface_install_property (iface,
		           g_param_spec_string ("name", "Name", "Name for the backend",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_string ("label", "Label", "Label for the backend",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_string ("description", "Description", "Description for the backend",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_object ("actions", "Actions", "Backend actions",
		                                GTK_TYPE_ACTION_GROUP, G_PARAM_READABLE));

		initialized = TRUE;
	}
}

SeahorsePlace *
seahorse_backend_lookup_place (SeahorseBackend *backend,
                               const gchar *uri)
{
	SeahorseBackendIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_BACKEND (backend), NULL);

	iface = SEAHORSE_BACKEND_GET_INTERFACE (backend);
	g_return_val_if_fail (iface->lookup_place != NULL, NULL);

	return (iface->lookup_place) (backend, uri);
}

void
seahorse_backend_register (SeahorseBackend *backend)
{
	gchar *name = NULL;

	g_return_if_fail (SEAHORSE_IS_BACKEND (backend));

	g_object_get (backend, "name", &name, NULL);
	seahorse_registry_register_object (NULL, G_OBJECT (backend), "backend", name, NULL);
	g_free (name);
}

GList *
seahorse_backend_get_registered (void)
{
	return seahorse_registry_object_instances (NULL, "backend", NULL);
}
