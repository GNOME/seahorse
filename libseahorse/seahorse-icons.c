/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include <gtk/gtk.h>

#include "seahorse-icons.h"

/**
 * seahorse_icons_init:
 *
 * Adds the icons to the icon theme
 *
 */
void
seahorse_icons_init (void)
{
	static gboolean icons_initted = FALSE;

	if (icons_initted)
		return;
	icons_initted = TRUE;

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   PKGDATADIR G_DIR_SEPARATOR_S "icons");
	gtk_window_set_default_icon_name ("seahorse");
}
