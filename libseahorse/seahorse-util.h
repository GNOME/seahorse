/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/**
 * A bunch of miscellaneous utility functions.
 */

#ifndef __SEAHORSE_UTIL_H__
#define __SEAHORSE_UTIL_H__

#include <gtk/gtk.h>
#include <time.h>

#define         SEAHORSE_ERROR                          (seahorse_util_error_domain ())

GQuark          seahorse_util_error_domain              (void);

unsigned int    seahorse_util_read_data_block           (GString      *buf,
                                                         GInputStream *input,
                                                         const char   *start,
                                                         const char   *end);

gboolean        seahorse_util_print_fd                  (int         fd,
                                                         const char *data);

gboolean        seahorse_util_printf_fd                 (int         fd,
                                                         const char *fmt,
                                                         ...);

guint       seahorse_ulong_hash    (gconstpointer v);

gboolean    seahorse_ulong_equal   (gconstpointer v1,
                                    gconstpointer v2);

#define     seahorse_util_wait_until(expr)                \
    while (!(expr)) {                                     \
        while (g_main_context_pending(NULL) && !(expr))   \
            g_main_context_iteration (NULL, FALSE);       \
        g_thread_yield ();                                \
    }

#endif /* __SEAHORSE_UTIL_H__ */
