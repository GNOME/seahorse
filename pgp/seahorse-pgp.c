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

#include "seahorse-pgp.h"

#include "seahorse-pgp-source.h"

#ifdef WITH_LDAP
#include "seahorse-ldap-source.h"
#endif
#ifdef WITH_HKP
#include "seahorse-hkp-source.h"
#endif
	
const SeahorseRegisterType SEAHORSE_PGP_REGISTRY[] = {
	seahorse_pgp_source_get_type,
#ifdef WITH_LDAP
	seahorse_ldap_source_get_type,
#endif
#ifdef WITH_HKP
	seahorse_hkp_source_get_type,
#endif
	NULL
};
