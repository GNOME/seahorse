/*
 *  Seahorse
 * 
 *  Copyright (C) 2005 Tecsidel S.A.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Fernando Herrera <fernando.herrera@tecsidel.es>
 * 
 */

#include <config.h>
#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-column-provider.h>
#include <glib/gi18n-lib.h>
#include "seahorse-nautilus.h"

/* -----------------------------------------------------------------------------
 * Nautilus Callbacks
 */
 
void
nautilus_module_initialize (GTypeModule *module)
{
    seahorse_nautilus_register_type (module);
	g_printerr("seahorse nautilus module initialized\n");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

void
nautilus_module_shutdown (void)
{
	g_printerr("seahorse nautilus module shutdown\n");
}

void 
nautilus_module_list_types (const GType **types, int *num_types)
{
	static GType type_list[1];
	
	type_list[0] = SEAHORSE_TYPE_NAUTILUS;
	*types = type_list;

	*num_types = 1;
}
