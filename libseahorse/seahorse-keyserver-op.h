/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

#ifndef __SEAHORSE_KEYSERVER_OP_H__
#define __SEAHORSE_KEYSERVER_OP_H__

#include <glib.h>

#include "seahorse-gpgmex.h"
#include "seahorse-operation.h"
#include "seahorse-key-source.h"

SeahorseOperation*  seahorse_keyserver_op_import     (SeahorseKeySource  *sksrc,
                                                      GList              *keys,
                                                      const gchar        *server);

SeahorseOperation*  seahorse_keyserver_op_retrieve   (GList              *keys,
                                                      const gchar        *server);

#endif /* __SEAHORSE_OP_H__ */
