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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

/**
 * A gpgme_data_t implementation which maps to a gio handle.
 * Allows for accessing data on remote machines (ie: smb, sftp)
 */
 
#ifndef __SEAHORSE_GPGME_IO__
#define __SEAHORSE_GPGME_IO__

#include <gpgme.h>
#include <gio/gio.h>

gpgme_data_t        seahorse_gpgme_input_data           (GInputStream* input);
gpgme_data_t        seahorse_gpgme_output_data          (GOutputStream* output);

#endif /* __SEAHORSE_GPGME_IO__ */
