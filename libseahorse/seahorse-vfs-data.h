/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SEAHORSE_VFS_DATA__
#define __SEAHORSE_VFS_DATA__

#define SEAHORSE_VFS_READ   0x00000000
#define SEAHORSE_VFS_WRITE  0x00000001
#define SEAHORSE_VFS_DELAY  0x00000010

gpgme_data_t seahorse_vfs_data_create (const gchar *uri, guint mode, 
                                       gpg_error_t* err);

#endif /* __SEAHORSE_VFS_DATA__ */
