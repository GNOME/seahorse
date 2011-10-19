/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 */

#ifndef __SEAHORSE_ACTION_H__
#define __SEAHORSE_ACTION_H__

#include <gtk/gtk.h>

GtkWindow *           seahorse_action_get_window                (GtkAction *action);

gpointer              seahorse_action_get_object                (GtkAction *action);

GList *               seahorse_action_get_objects               (GtkAction *action);

void                  seahorse_action_set_window                (GtkAction *action,
                                                                 GtkWindow *window);

void                  seahorse_action_set_objects               (GtkAction *action,
                                                                 GList *objects);

gboolean              seahorse_action_have_objects              (GtkAction *action);

#endif
