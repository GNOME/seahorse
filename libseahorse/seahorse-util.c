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

#include "config.h"

#include "seahorse-common.h"

#include "seahorse-util.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib/gi18n.h>

#include <sys/types.h>

/**
 * seahorse_util_error_domain:
 *
 * Returns: The GError domain for generic seahorse errors
 **/
GQuark
seahorse_util_error_domain ()
{
    static GQuark q = 0;
    if(q == 0)
        q = g_quark_from_static_string ("seahorse-error");
    return q;
}

/**
 * seahorse_util_print_fd:
 * @fd: The file descriptor to write to
 * @s:  The data to write
 *
 * Returns: FALSE on error, TRUE on success
 */
gboolean
seahorse_util_print_fd (int fd, const char* s)
{
    /* Guarantee all data is written */
    int r, l = strlen (s);

    while (l > 0) {
        r = write (fd, s, l);

        if (r == -1) {
            if (errno == EPIPE)
                return FALSE;
            if (errno != EAGAIN && errno != EINTR) {
                g_critical ("couldn't write data to socket: %s", strerror (errno));
                return FALSE;
            }

        } else {
            s += r;
            l -= r;
        }
    }

    return TRUE;
}

/**
 * seahorse_util_printf_fd:
 * @fd: The file descriptor to write to
 * @fmt: The printf format of the data to write
 * @...: The parameters to insert
 *
 * Returns: TRUE on success, FALSE on error
 */
gboolean
seahorse_util_printf_fd (int fd, const char* fmt, ...)
{
    g_autofree char *t = NULL;
    va_list ap;

    va_start (ap, fmt);
    t = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    return seahorse_util_print_fd (fd, t);
}

/**
 * seahorse_util_read_data_block:
 * @buf: A string buffer to write the data to.
 * @input: The input stream to read from.
 * @start: The start signature to look for.
 * @end: The end signature to look for.
 *
 * Breaks out one block of data (usually a key)
 *
 * Returns: The number of bytes copied.
 */
guint
seahorse_util_read_data_block (GString      *buf,
                               GInputStream *input,
                               const char   *start,
                               const char   *end)
{
    const char *t;
    guint copied = 0;
    char ch;
    gsize read;

    /* Look for the beginning */
    t = start;
    while (g_input_stream_read_all (input, &ch, 1, &read, NULL, NULL) && read == 1) {
        /* Match next char */
        if (*t == ch)
            t++;

        /* Did we find the whole string? */
        if (!*t) {
            buf = g_string_append (buf, start);
            copied += strlen (start);
            break;
        }
    }

    /* Look for the end */
    t = end;
    while (g_input_stream_read_all (input, &ch, 1, &read, NULL, NULL) && read == 1) {
        /* Match next char */
        if (*t == ch)
            t++;

        buf = g_string_append_c (buf, ch);
        copied++;

        /* Did we find the whole string? */
        if (!*t)
            break;
    }

    return copied;
}

/**
 * seahorse_util_parse_version:
 *
 * @version: Version number string in the form xx.yy.zz
 *
 * Converts an (up to) four-part version number into a 64-bit
 * unsigned integer for simple comparison.
 *
 * Returns: SeahorseVersion
 **/
SeahorseVersion
seahorse_util_parse_version (const char *version)
{
	SeahorseVersion ret = 0, tmp = 0;
	int offset = 48;
	gchar **tokens = g_strsplit (version, ".", 5);
	int i;
	for (i=0; tokens[i] && offset >= 0; i++) {
		tmp = atoi(tokens[i]);
		ret += tmp << offset;
		offset -= 16;
	}
	g_strfreev(tokens);
	return ret;
}

guint
seahorse_ulong_hash (gconstpointer v)
{
	const signed char *p = v;
	guint32 i, h = *p;

	for(i = 0; i < sizeof (gulong); ++i)
		h = (h << 5) - h + *(p++);

	return h;
}

gboolean
seahorse_ulong_equal (gconstpointer v1,
                             gconstpointer v2)
{
	return *((const gulong*)v1) == *((const gulong*)v2);
}
