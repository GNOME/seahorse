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

#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-item-commands.h"
#include "seahorse-gkr-keyring-commands.h"
#include "seahorse-gkr-source.h"

#include "seahorse-context.h"

#include <gnome-keyring.h>

void
seahorse_gkr_module_init (void)
{
	SeahorseSource *source;

	source = SEAHORSE_SOURCE (seahorse_gkr_source_new ());
	seahorse_context_take_source (NULL, source);
	
	/* Let these classes register themselves */
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_GKR_SOURCE));
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_GKR_ITEM_COMMANDS));
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_GKR_KEYRING_COMMANDS));
}
