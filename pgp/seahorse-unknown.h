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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include "seahorse-common.h"

/* Solve a circular include */
typedef struct _SeahorseUnknownSource SeahorseUnknownSource;


#define SEAHORSE_TYPE_UNKNOWN (seahorse_unknown_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseUnknown, seahorse_unknown, SEAHORSE, UNKNOWN, SeahorseObject)

SeahorseUnknown *    seahorse_unknown_new              (SeahorseUnknownSource *usrc,
                                                        const char *keyid,
                                                        const char *display);

const char *         seahorse_unknown_get_keyid        (SeahorseUnknown *self);
