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

#ifndef __SEAHORSE_UTIL_H__
#define __SEAHORSE_UTIL_H__

#include <gtk/gtk.h>
#include <time.h>
#include <gpgme.h>

typedef enum {
	SEAHORSE_CRYPT_SUFFIX,
	SEAHORSE_SIG_SUFFIX,
	SEAHORSE_ASC_SUFFIX
} SeahorseSuffix;

void		seahorse_util_show_error		(GtkWindow		*parent,
							 const gchar 		*message);

gchar*		seahorse_util_get_text_view_text	(GtkTextView		*view);

void		seahorse_util_set_text_view_string	(GtkTextView		*view,
							 GString		*string);

gchar*		seahorse_util_get_date_string		(const time_t		time);

void		seahorse_util_handle_error		(GpgmeError		err);

GpgmeError	seahorse_util_write_data_to_file	(const gchar		*path,
							 GpgmeData		data);

gchar*		seahorse_util_write_data_to_text	(GpgmeData		data);

gboolean	seahorse_util_check_suffix		(const gchar		*path,
							 SeahorseSuffix		suffix);

gchar*		seahorse_util_add_suffix		(GpgmeCtx		ctx,
							 const gchar		*path,
							 SeahorseSuffix		suffix);

gchar*		seahorse_util_remove_suffix		(const gchar		*path);

#endif /* __SEAHORSE_UTIL_H__ */
