/*
 * Copyright (C) 2007 Nokia Corporation
 * Copyright (C) 2007-2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef SEAHORSE_DEBUG_H
#define SEAHORSE_DEBUG_H

#include "config.h"

#include <glib.h>

G_BEGIN_DECLS

/* Please keep this enum in sync with #keys in seahorse-debug.c */
typedef enum {
	SEAHORSE_DEBUG_KEYS         = 1 << 1,
	SEAHORSE_DEBUG_LOAD         = 1 << 2,
	SEAHORSE_DEBUG_DNSSD        = 1 << 3,
	SEAHORSE_DEBUG_LDAP         = 1 << 4,
	SEAHORSE_DEBUG_HKP          = 1 << 5,
	SEAHORSE_DEBUG_OPERATION    = 1 << 6,
	SEAHORSE_DEBUG_DRAG         = 1 << 7,
} SeahorseDebugFlags;

gboolean    seahorse_debug_flag_is_set              (SeahorseDebugFlags flag);

void        seahorse_debug_set_flags                (const gchar *flags_string);

void        seahorse_debug_message                  (SeahorseDebugFlags flag,
                                                     const gchar *format,
                                                     ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS

#endif /* SEAHORSE_DEBUG_H */

/* -----------------------------------------------------------------------------
 * Below this point is outside the SEAHORSE_DEBUG_H guard - so it can take effect
 * more than once. So you can do:
 *
 * #define DEBUG_FLAG SEAHORSE_DEBUG_ONE_THING
 * #include "seahorse-debug.h"
 * ...
 * seahorse_debug ("if we're debugging one thing");
 * ...
 * #undef DEBUG_FLAG
 * #define DEBUG_FLAG SEAHORSE_DEBUG_OTHER_THING
 * #include "seahorse-debug.h"
 * ...
 * seahorse_debug ("if we're debugging the other thing");
 * ...
 */

#ifdef DEBUG_FLAG
#ifdef WITH_DEBUG

#undef seahorse_debug
#define seahorse_debug(format, ...) \
	seahorse_debug_message (DEBUG_FLAG, "%s: " format, G_STRFUNC, ##__VA_ARGS__)

#undef seahorse_debugging
#define seahorse_debugging \
	seahorse_debug_flag_is_set (DEBUG_FLAG)

#else /* !defined (WITH_DEBUG) */

#undef seahorse_debug
#define seahorse_debug(format, ...) \
	do {} while (0)

#undef seahorse_debugging
#define seahorse_debugging 0

#endif /* !defined (WITH_DEBUG) */

#endif /* defined (DEBUG_FLAG) */
