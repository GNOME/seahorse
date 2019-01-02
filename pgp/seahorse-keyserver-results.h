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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "seahorse-common.h"

G_BEGIN_DECLS


#define SEAHORSE_TYPE_KEYSERVER_RESULTS (seahorse_keyserver_results_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseKeyserverResults, seahorse_keyserver_results, SEAHORSE, KEYSERVER_RESULTS, GtkDialog)

void             seahorse_keyserver_results_show             (const gchar *search_text, GtkWindow *parent);

const gchar*     seahorse_keyserver_results_get_search       (SeahorseKeyserverResults* self);


G_END_DECLS
