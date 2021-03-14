/*
 * Seahorse
 *
 * Copyright (C) 2003 Stefan Walter
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

/**
 * A collection of functions for changing options in gpg.conf.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

const char *   seahorse_gpg_homedir             (void);

gboolean       seahorse_gpg_options_find        (const char  *option,
                                                 char       **value,
                                                 GError     **err);

gboolean       seahorse_gpg_options_find_vals   (const char  *options[],
                                                 char        *values[],
                                                 GError     **err);

gboolean       seahorse_gpg_options_change      (const char  *option,
                                                 const char  *value,
                                                 GError     **err);

gboolean       seahorse_gpg_options_change_vals (const char  *options[],
                                                 char        *values[],
                                                 GError     **err);

G_BEGIN_DECLS
