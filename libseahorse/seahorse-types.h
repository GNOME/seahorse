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

#include "libcryptui/cryptui.h"

G_BEGIN_DECLS


#define SEAHORSE_TYPE_LOCATION (seahorse_location_get_type ())

#define SEAHORSE_TYPE_USAGE (seahorse_usage_get_type ())

/* 
 * These types should never change. These values are exported via DBUS. In the 
 * case of a key being in multiple locations, the highest location always 'wins'.
 */
typedef enum  {
	SEAHORSE_LOCATION_INVALID = 0,
	SEAHORSE_LOCATION_MISSING = 10,
	SEAHORSE_LOCATION_SEARCHING = 20,
	SEAHORSE_LOCATION_REMOTE = 50,
	SEAHORSE_LOCATION_LOCAL = 100
} SeahorseLocation;

GType seahorse_location_get_type (void);

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

GType seahorse_usage_get_type (void);

typedef enum {
	SEAHORSE_FLAG_IS_VALID =    CRYPTUI_FLAG_IS_VALID,
	SEAHORSE_FLAG_CAN_ENCRYPT = CRYPTUI_FLAG_CAN_ENCRYPT,
	SEAHORSE_FLAG_CAN_SIGN =    CRYPTUI_FLAG_CAN_SIGN,
	SEAHORSE_FLAG_EXPIRED =     CRYPTUI_FLAG_EXPIRED,
	SEAHORSE_FLAG_REVOKED =     CRYPTUI_FLAG_REVOKED,
	SEAHORSE_FLAG_DISABLED =    CRYPTUI_FLAG_DISABLED,
	SEAHORSE_FLAG_TRUSTED =     CRYPTUI_FLAG_TRUSTED,
	SEAHORSE_FLAG_EXPORTABLE =  CRYPTUI_FLAG_EXPORTABLE,
	SEAHORSE_FLAG_DELETABLE = 1000
} SeahorseKeyFlags;

#define SEAHORSE_TAG_INVALID               0

G_END_DECLS

#endif
