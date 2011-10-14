/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#ifndef __SEAHORSE_TYPES_H__
#define __SEAHORSE_TYPES_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS


#define SEAHORSE_TYPE_USAGE (seahorse_usage_get_type ())

/* Again, never change these values */
typedef enum  {
	SEAHORSE_USAGE_NONE = 0,
	SEAHORSE_USAGE_SYMMETRIC_KEY = 1,
	SEAHORSE_USAGE_PUBLIC_KEY = 2,
	SEAHORSE_USAGE_PRIVATE_KEY = 3,
	SEAHORSE_USAGE_CREDENTIALS = 4,
	SEAHORSE_USAGE_IDENTITY = 5,
	SEAHORSE_USAGE_OTHER = 10
} SeahorseUsage;

GType seahorse_usage_get_type (void) G_GNUC_CONST;

#define SEAHORSE_TYPE_FLAGS (seahorse_flags_get_type ())

typedef enum {
	SEAHORSE_FLAG_NONE = 0,
	SEAHORSE_FLAG_IS_VALID =    0x00000001,
	SEAHORSE_FLAG_CAN_ENCRYPT = 0x00000002,
	SEAHORSE_FLAG_CAN_SIGN =    0x00000004,
	SEAHORSE_FLAG_EXPIRED =     0x00000100,
	SEAHORSE_FLAG_REVOKED =     0x00000200,
	SEAHORSE_FLAG_DISABLED =    0x00000400,
	SEAHORSE_FLAG_TRUSTED =     0x00001000,
	SEAHORSE_FLAG_PERSONAL =    0x00002000,
	SEAHORSE_FLAG_EXPORTABLE =  0x00100000,
	SEAHORSE_FLAG_DELETABLE =   0x10000000
} SeahorseFlags;

GType seahorse_flags_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
