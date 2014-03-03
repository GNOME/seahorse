/*
 * Seahorse
 *
 * Copyright (C) 2006 Adam Schreiber
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

#ifndef __SEAHORSE_KEYSERVER_SYNC_H__
#define __SEAHORSE_KEYSERVER_SYNC_H__

#include <gtk/gtk.h>

void        seahorse_keyserver_sync             (GList *keys);


GtkWindow*  seahorse_keyserver_sync_show        (GList *keys,
                                                 GtkWindow *parent);

#endif
