/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

#ifndef __SEAHORSE_GEDIT_H__
#define __SEAHORSE_GEDIT_H__

#include "config.h"

void            seahorse_gedit_encrypt          (GeditDocument *doc);
void            seahorse_gedit_sign             (GeditDocument *doc);
void            seahorse_gedit_decrypt          (GeditDocument *doc);
void            seahorse_gedit_cleanup          ();

GtkWindow*      seahorse_gedit_active_window    (void);
void            seahorse_gedit_flash            (const gchar *format, ...);
void            seahorse_gedit_show_error       (const gchar *heading, GError *error);

#ifdef WITH_GEDIT_BONOBO
#define SEAHORSE_GEDIT_DEBUG     gedit_debug 
#else
#define SEAHORSE_GEDIT_DEBUG     gedit_debug_message
#endif

#endif /* __SEAHORSE_GEDIT_H__ */
