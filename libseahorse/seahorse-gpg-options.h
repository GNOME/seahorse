/*
 * Seahorse
 *
 * Copyright (C) 2003 Nate Nielsen
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

/** 
 * A collection of functions for changing options in gpg.conf.
 */

#ifndef __SEAHORSE_GPG_OPTIONS_H__
#define __SEAHORSE_GPG_OPTIONS_H__

#include <glib.h>

const gchar * seahorse_gpg_homedir ();

gboolean seahorse_gpg_options_find(const gchar* option, 
                                gchar** value, GError** err);
                                
gboolean seahorse_gpg_options_find_vals(const gchar* options[], 
                                gchar* values[], GError** err);

gboolean seahorse_gpg_options_change(const gchar* option, 
                                const gchar* value, GError** err);
                                
gboolean seahorse_gpg_options_change_vals(const gchar* options[], 
                                gchar* values[], GError** err);

#endif /* __SEAHORSE_WIDGET_H__ */
