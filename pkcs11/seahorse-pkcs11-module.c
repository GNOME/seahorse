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

#include "seahorse-pkcs11-module.h"

#include "seahorse-pkcs11-commands.h"
#include "seahorse-pkcs11-source.h"

#include "seahorse-util.h"

#include <gck/gck.h>

void
seahorse_pkcs11_module_init (void)
{
	SeahorseSource *source;
	GList *slots, *s;
	GList *modules, *m;

	modules = gck_modules_initialize_registered ();
	for (m = modules; m != NULL; m = g_list_next (m)) {
		slots = gck_module_get_slots (m->data, FALSE);
		for (s = slots; s; s = g_list_next (s)) {
			source = SEAHORSE_SOURCE (seahorse_pkcs11_source_new (s->data));
			seahorse_context_take_source (NULL, source);
		}

		/* These will have been refed by the source above */
		gck_list_unref_free (slots);
	}

	gck_list_unref_free (modules);

	/* Let these register themselves */
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_PKCS11_SOURCE));
	g_type_class_unref (g_type_class_ref (SEAHORSE_PKCS11_TYPE_COMMANDS));
}
