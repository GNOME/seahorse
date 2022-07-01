/*
 * Seahorse
 *
 * Copyright (C) 2021 Niels De Graef
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

#include "seahorse-pgp-uid.h"

#define SEAHORSE_PGP_TYPE_UID_LIST_BOX (seahorse_pgp_uid_list_box_get_type ())
G_DECLARE_FINAL_TYPE (SeahorsePgpUidListBox, seahorse_pgp_uid_list_box,
                      SEAHORSE_PGP, UID_LIST_BOX,
                      AdwPreferencesGroup)

#define SEAHORSE_PGP_TYPE_UID_LIST_BOX_ROW (seahorse_pgp_uid_list_box_row_get_type ())
G_DECLARE_FINAL_TYPE (SeahorsePgpUidListBoxRow, seahorse_pgp_uid_list_box_row,
                      SEAHORSE_PGP, UID_LIST_BOX_ROW,
                      AdwExpanderRow)

GtkWidget * seahorse_pgp_uid_list_box_new (SeahorsePgpKey *key);
