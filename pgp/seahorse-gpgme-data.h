/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

/**
 * A gpgme_data_t implementation which maps to a gio handle.
 * Allows for accessing data on remote machines (ie: smb, sftp)
 */

#pragma once

#include <gpgme.h>
#include <gio/gio.h>

gpgme_data_t        seahorse_gpgme_data_input           (GInputStream* input);

gpgme_data_t        seahorse_gpgme_data_output          (GOutputStream* output);

/*
 * GTK/Glib use a model where if allocation fails, the program exits. These
 * helper functions extend certain GPGME calls to provide the same behavior.
 */

gpgme_data_t        seahorse_gpgme_data_new          (void);

void                seahorse_gpgme_data_release      (gpgme_data_t data);

int                 seahorse_gpgme_data_write_all    (gpgme_data_t data, const void* buffer, size_t len);

gpgme_data_t        seahorse_gpgme_data_new_from_mem (const char *buffer, size_t size, gboolean copy);
