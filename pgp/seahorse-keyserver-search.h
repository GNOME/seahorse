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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#pragma once

#include <adwaita.h>

#define SEAHORSE_TYPE_KEYSERVER_SEARCH (seahorse_keyserver_search_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseKeyserverSearch, seahorse_keyserver_search,
                      SEAHORSE, KEYSERVER_SEARCH,
                      AdwDialog)

SeahorseKeyserverSearch *     seahorse_keyserver_search_new             (void);

char *                        seahorse_keyserver_search_get_search_text (SeahorseKeyserverSearch *self);
