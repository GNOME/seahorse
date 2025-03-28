/*
 * Seahorse
 *
 * Copyright (C) 2022 Niels De Graef
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

#include "pgp/seahorse-pgp-key.h"

#define SEAHORSE_PGP_TYPE_KEY_PANEL (seahorse_pgp_key_panel_get_type ())
G_DECLARE_FINAL_TYPE (SeahorsePgpKeyPanel, seahorse_pgp_key_panel,
                      SEAHORSE_PGP, KEY_PANEL,
                      SeahorsePanel);

GtkWidget *     seahorse_pgp_key_panel_new    (SeahorsePgpKey *pkey);
