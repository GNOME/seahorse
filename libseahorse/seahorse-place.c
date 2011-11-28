/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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

#include "seahorse-context.h"
#include "seahorse-marshal.h"
#include "seahorse-object.h"
#include "seahorse-registry.h"
#include "seahorse-place.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

/**
 * SECTION:seahorse-place
 * @short_description: This class stores and handles key places
 * @include:seahorse-place.h
 *
 **/

typedef SeahorsePlaceIface SeahorsePlaceInterface;

G_DEFINE_INTERFACE (SeahorsePlace, seahorse_place, GCR_TYPE_COLLECTION);

/* ---------------------------------------------------------------------------------
 * INTERFACE
 */

/**
* gobject_class: The object class to init
*
**/
static void
seahorse_place_default_init (SeahorsePlaceIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		g_object_interface_install_property (iface,
		           g_param_spec_string ("label", "Label", "Label for the place",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_string ("description", "Description", "Description for the place",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_string ("uri", "URI", "URI for the place",
		                                "", G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_object ("icon", "Icon", "Icon for this place",
		                                G_TYPE_ICON, G_PARAM_READABLE));

		g_object_interface_install_property (iface,
		           g_param_spec_object ("actions", "Actions", "Actions for place",
		                                GTK_TYPE_ACTION_GROUP, G_PARAM_READABLE));

		initialized = TRUE;
	}
}

/* ---------------------------------------------------------------------------------
 * PUBLIC
 */

void
seahorse_place_import_async (SeahorsePlace *place,
                             GInputStream *input,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	g_return_if_fail (SEAHORSE_IS_PLACE (place));
	g_return_if_fail (G_IS_INPUT_STREAM (input));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (SEAHORSE_PLACE_GET_INTERFACE (place)->import_async);
	SEAHORSE_PLACE_GET_INTERFACE (place)->import_async (place, input, cancellable,
	                                                    callback, user_data);
}

GList *
seahorse_place_import_finish (SeahorsePlace *place,
                              GAsyncResult *result,
                              GError **error)
{
	g_return_val_if_fail (SEAHORSE_IS_PLACE (place), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	g_return_val_if_fail (SEAHORSE_PLACE_GET_INTERFACE (place)->import_finish, NULL);
	return SEAHORSE_PLACE_GET_INTERFACE (place)->import_finish (place, result, error);
}
