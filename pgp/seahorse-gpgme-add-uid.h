/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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
#include "seahorse-gpgme-key.h"

#define SEAHORSE_GPGME_TYPE_ADD_UID (seahorse_gpgme_add_uid_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseGpgmeAddUid, seahorse_gpgme_add_uid,
                      SEAHORSE_GPGME, ADD_UID,
                      GtkApplicationWindow)

SeahorseGpgmeAddUid * seahorse_gpgme_add_uid_new        (SeahorseGpgmeKey *pkey,
                                                         GtkWindow *parent);

void                  seahorse_gpgme_add_uid_run_dialog (SeahorseGpgmeKey *pkey,
                                                         GtkWindow *parent);
