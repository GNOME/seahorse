/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "seahorse-viewable.h"

typedef SeahorseViewableIface SeahorseViewableInterface;

G_DEFINE_INTERFACE (SeahorseViewable, seahorse_viewable, G_TYPE_OBJECT);

void
seahorse_viewable_show_viewer (SeahorseViewable *viewable,
                               GtkWindow *parent)
{
	SeahorseViewableIface *iface;

	g_return_if_fail (SEAHORSE_IS_VIEWABLE (viewable));
	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

	iface = SEAHORSE_VIEWABLE_GET_INTERFACE (viewable);
	g_return_if_fail (iface->show_viewer != NULL);

	return iface->show_viewer (viewable, parent);
}

static void
seahorse_viewable_default_init (SeahorseViewableIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		initialized = TRUE;
	}
}

gboolean
seahorse_viewable_can_view (gpointer object)
{
	return SEAHORSE_IS_VIEWABLE (object);
}
