/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

/* Miscellaneos utility functions used in multiple widgets or functions */

#include <gtk/gtk.h>
#include <time.h>

/* Runs an error dialog with the given message */
void		seahorse_util_show_error		(GtkWindow	*parent,
							 const gchar 	*message);

/* Returns the text contained in the text view */
gchar*		seahorse_util_get_text_view_text	(GtkTextView	*view);

/* Sets the text view's text to the text contained in string */
void		seahorse_util_set_text_view_string	(GtkTextView	*view,
							 GString	*string);

/* Returns a gpgme compatible string representation of the time */
const gchar*	seahorse_util_get_date_string		(const time_t	time);

#endif /* __SEAHORSE_UTIL_H__ */
