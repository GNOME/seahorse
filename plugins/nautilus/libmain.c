/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <config.h>
#include <string.h>
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>

#include "seahorse-component.h"

#define VIEW_IID_ALL "OAFIID:Seahorse_Component_All"
#define VIEW_IID_ENCRYPTED "OAFIID:Seahorse_Component_Encrypted"
#define VIEW_IID_SIGNATURE "OAFIID:Seahorse_Component_Signature"
#define VIEW_IID_KEYS "OAFIID:Seahorse_Component_Keys"

static CORBA_Object
image_shlib_make_object (PortableServer_POA poa, const gchar *iid,
			 gpointer impl_ptr, CORBA_Environment *ev)
{
	SeahorseComponent *sc;

	sc = g_object_new (SEAHORSE_TYPE_COMPONENT, NULL);
	bonobo_activation_plugin_use (poa, impl_ptr);
	return CORBA_Object_duplicate (BONOBO_OBJREF (sc), ev);
}

static const BonoboActivationPluginObject plugin_list[] = {
	{ VIEW_IID_ALL, image_shlib_make_object },
	{ VIEW_IID_ENCRYPTED, image_shlib_make_object },
	{ VIEW_IID_SIGNATURE, image_shlib_make_object },
	{ VIEW_IID_KEYS, image_shlib_make_object },
	{ NULL }
};

const BonoboActivationPlugin Bonobo_Plugin_info = {
	plugin_list, "Seahorse Component"
};
