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

#include "seahorse-gkr-module.h"

#include "seahorse-gkr-commands.h"
#include "seahorse-gkeyring-source.h"

#include "seahorse-context.h"

void
seahorse_gkr_module_init (void)
{
	SeahorseSource *source;

	/* Always have a default keyring source added */
	source = g_object_new (SEAHORSE_TYPE_GKEYRING_SOURCE, NULL);
	seahorse_context_take_source (NULL, source);

	/* Let these classes register themselves */
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_GKEYRING_SOURCE));
	g_type_class_unref (g_type_class_ref (SEAHORSE_GKEYRING_TYPE_COMMANDS));
}
