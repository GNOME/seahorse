/*
 * Seahorse
 *
 * Copyright (C) 2004-2006 Stefan Walter
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
 * seahorse_combo_keys_*: Shows a list of keys in a dropdown for selection.
 *
 * - Attaches to a GtkComboBox
 * - Gets its list of keys from a SeahorseSet.
 */

#ifndef __SEAHORSE_COMBO_KEYS_H__
#define __SEAHORSE_COMBO_KEYS_H__

#include "seahorse-pgp-key.h"

#include <gtk/gtk.h>
#include <gcr/gcr.h>

void                        seahorse_combo_keys_attach              (GtkComboBox *combo,
                                                                     GcrCollection *collection,
                                                                     const gchar *none_option);

void                        seahorse_combo_keys_set_active_id       (GtkComboBox *combo,
                                                                     const gchar *keyid);

void                        seahorse_combo_keys_set_active          (GtkComboBox *combo,
                                                                     SeahorsePgpKey *key);

SeahorsePgpKey *            seahorse_combo_keys_get_active          (GtkComboBox *combo);

const gchar *               seahorse_combo_keys_get_active_id       (GtkComboBox *combo);

#endif /* __SEAHORSE_COMBO_KEYS_H__ */
