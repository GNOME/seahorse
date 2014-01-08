/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/**
 * A gpgme_data_t implementation which maps to a gio handle.
 * Allows for accessing data on remote machines (ie: smb, sftp)
 */
 
#ifndef __SEAHORSE_GPGME_IO__
#define __SEAHORSE_GPGME_IO__

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

#endif /* __SEAHORSE_GPGME_IO__ */
