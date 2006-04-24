/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#ifndef __SEAHORSE_WINDOWS_H__
#define __SEAHORSE_WINDOWS_H__

#include <gpgme.h>

#include "seahorse-context.h"

GtkWindow*	seahorse_key_manager_show	        (SeahorseOperation *op);

GtkWindow*  seahorse_keyserver_search_show      (void);

GtkWindow*  seahorse_keyserver_sync_show        (GList *keys);

GtkWindow*  seahorse_keyserver_results_show     (SeahorseOperation *op,
                                                 const gchar *search_text);

void        seahorse_generate_select_show       (void);

void        seahorse_generate_adv_show          (void);

void        seahorse_generate_druid_show        (void);

void        seahorse_delete_show                (GList *keys);

#endif /* __SEAHORSE_WINDOWS_H__ */
