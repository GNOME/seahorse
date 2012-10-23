/*
 * Seahorse
 *
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_VIEWABLE_H__
#define __SEAHORSE_VIEWABLE_H__

#include "seahorse-common.h"

#include <gio/gio.h>

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_VIEWABLE                (seahorse_viewable_get_type ())
#define SEAHORSE_VIEWABLE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_VIEWABLE, SeahorseViewable))
#define SEAHORSE_IS_VIEWABLE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_VIEWABLE))
#define SEAHORSE_VIEWABLE_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_VIEWABLE, SeahorseViewableIface))

typedef struct _SeahorseViewable SeahorseViewable;
typedef struct _SeahorseViewableIface SeahorseViewableIface;

struct _SeahorseViewableIface {
	GTypeInterface parent;

	void           (* show_viewer)      (SeahorseViewable *viewable,
	                                     GtkWindow *parent);
};

GType              seahorse_viewable_get_type               (void) G_GNUC_CONST;

void               seahorse_viewable_show_viewer            (SeahorseViewable *viewable,
                                                             GtkWindow *parent);

gboolean           seahorse_viewable_can_view               (gpointer object);

#endif /* __SEAHORSE_VIEWABLE_H__ */
