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

/**
 * Soon to be removed. 
 */
 
#ifndef __SEAHORSE_OP_H__
#define __SEAHORSE_OP_H__

#include <glib.h>
#include <gpgme.h>

#include "seahorse-pgp-source.h"

gint        seahorse_op_import_file         (SeahorsePGPSource  *psrc,
                                             const gchar        *path,
                                             GError             **err);

gint        seahorse_op_import_text         (SeahorsePGPSource  *psrc,
                                             const gchar        *text,
                                             GError             **err);

#endif /* __SEAHORSE_OP_H__ */
