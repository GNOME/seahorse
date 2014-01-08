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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __SEAHORSE_SSH_H__
#define __SEAHORSE_SSH_H__

#include <glib.h>
#include <glib-object.h>

#include <gcr/gcr.h>

#define SEAHORSE_SSH_NAME "openssh"
#define SEAHORSE_SSH_TYPE_STR SEAHORSE_SSH_NAME
#define SEAHORSE_SSH (g_quark_from_static_string (SEAHORSE_SSH_NAME))

#ifdef WITH_SSH

void       seahorse_ssh_backend_initialize    (void);

#endif /* WITH_SSH */

#endif
