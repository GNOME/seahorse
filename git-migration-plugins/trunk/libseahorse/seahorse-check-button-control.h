/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 
#ifndef __SEAHORSE_CHECK_BUTTON_CONTROL_H__
#define __SEAHORSE_CHECK_BUTTON_CONTROL_H__

#include <gtk/gtk.h>

/* An API for dealing with elsewhere created widgets */

void        seahorse_check_button_gconf_attach      (GtkCheckButton *button,
                                                     const char *gconf_key);

void        seahorse_check_button_gconf_detach      (GtkCheckButton *button);

#endif /* __SEAHORSE_CHECK_BUTTON_CONTROL_H__ */
