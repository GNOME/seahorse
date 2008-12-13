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

#include "seahorse-pgp-module.h"

#include "seahorse-pgp-commands.h"
#include "seahorse-pgp-generator.h"
#include "seahorse-pgp-source.h"

#ifdef WITH_LDAP
#include "seahorse-ldap-source.h"
#endif
#ifdef WITH_HKP
#include "seahorse-hkp-source.h"
#endif

#include "seahorse-context.h"
	
void
seahorse_pgp_module_init (void)
{
	SeahorseSource *source;

	/* Always have a default pgp source added */
	source = g_object_new (SEAHORSE_TYPE_PGP_SOURCE, NULL);
	seahorse_context_take_source (NULL, source);

	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_PGP_SOURCE));
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_PGP_COMMANDS));
	g_type_class_unref (g_type_class_ref (SEAHORSE_PGP_TYPE_GENERATOR));
#ifdef WITH_LDAP
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_LDAP_SOURCE));
#endif
#ifdef WITH_HKP
	g_type_class_unref (g_type_class_ref (SEAHORSE_TYPE_HKP_SOURCE));
#endif 
}
