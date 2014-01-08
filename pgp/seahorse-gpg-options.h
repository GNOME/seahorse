/*
 * Seahorse
 *
 * Copyright (C) 2003 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/** 
 * A collection of functions for changing options in gpg.conf.
 */

#ifndef __SEAHORSE_GPG_OPTIONS_H__
#define __SEAHORSE_GPG_OPTIONS_H__

#include <glib.h>

const gchar * seahorse_gpg_homedir (void);

gboolean seahorse_gpg_options_find(const gchar* option, 
                                gchar** value, GError** err);
                                
gboolean seahorse_gpg_options_find_vals(const gchar* options[], 
                                gchar* values[], GError** err);

gboolean seahorse_gpg_options_change(const gchar* option, 
                                const gchar* value, GError** err);
                                
gboolean seahorse_gpg_options_change_vals(const gchar* options[], 
                                gchar* values[], GError** err);

#endif /* __SEAHORSE_WIDGET_H__ */
