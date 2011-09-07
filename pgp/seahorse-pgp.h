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

#ifndef __SEAHORSE_PGP_H__
#define __SEAHORSE_PGP_H__

#include <glib.h>
#include <glib-object.h>

#include <gcr/gcr.h>

G_BEGIN_DECLS

#ifdef WITH_PGP

#define SEAHORSE_PGP g_quark_from_string ("openpgp")
#define SEAHORSE_PGP_TYPE_STR "openpgp"
#define SEAHORSE_PGP_TYPE g_quark_from_string ("openpgp")
#define SEAHORSE_PGP_STOCK_ICON "seahorse-key-personal"

GcrCollection *       seahorse_pgp_backend_initialize    (void);

#endif /* WITH_PGP */

G_END_DECLS

#endif
