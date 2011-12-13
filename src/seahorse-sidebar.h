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
 */

#ifndef __SEAHORSE_SIDEBAR_H__
#define __SEAHORSE_SIDEBAR_H__

#include "seahorse-place.h"

#include <gcr/gcr.h>

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_SIDEBAR             (seahorse_sidebar_get_type ())
#define SEAHORSE_SIDEBAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SIDEBAR, SeahorseSidebar))
#define SEAHORSE_SIDEBAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SIDEBAR, SeahorseSidebarClass))
#define SEAHORSE_IS_SIDEBAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SIDEBAR))
#define SEAHORSE_IS_SIDEBAR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SIDEBAR))
#define SEAHORSE_SIDEBAR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SIDEBAR_KEY, SeahorseSidebarClass))

typedef struct _SeahorseSidebar SeahorseSidebar;
typedef struct _SeahorseSidebarClass SeahorseSidebarClass;

GType                        seahorse_sidebar_get_type           (void);

SeahorseSidebar *            seahorse_sidebar_new                (void);

gboolean                     seahorse_sidebar_get_combined       (SeahorseSidebar *self);

void                         seahorse_sidebar_set_combined       (SeahorseSidebar *self,
                                                                  gboolean combined);

GcrCollection *              seahorse_sidebar_get_collection     (SeahorseSidebar *self);

gchar **                     seahorse_sidebar_get_selected_uris  (SeahorseSidebar *self);

void                         seahorse_sidebar_set_selected_uris  (SeahorseSidebar *self,
                                                                  const gchar **value);

GList *                 seahorse_sidebar_get_selected_places     (SeahorseSidebar *self);

SeahorsePlace *         seahorse_sidebar_get_focused_place       (SeahorseSidebar *self);

GList *                 seahorse_sidebar_get_backends            (SeahorseSidebar *self);

#endif /* __SEAHORSE_SIDEBAR_H__ */
