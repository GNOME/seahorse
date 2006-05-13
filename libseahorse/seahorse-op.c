/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Nate Nielsen
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

#include <string.h>

#include "seahorse-gpgmex.h"
#include "seahorse-op.h"
#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-pgp-source.h"
#include "seahorse-pgp-key.h"
#include "seahorse-vfs-data.h"
#include "seahorse-gconf.h"

/* helper function for importing @data. @data will be released.
 * returns number of keys imported or -1. */
static gint
import_data (SeahorsePGPSource *psrc, gpgme_data_t data, 
             GError **err)
{
    SeahorseOperation *operation;
    GList *keylist;
    gint keys = 0;
    
    g_return_val_if_fail (!err || !err[0], -1);

    operation = seahorse_key_source_import (SEAHORSE_KEY_SOURCE (psrc), data);
    g_return_val_if_fail (operation != NULL, -1);
    
    seahorse_operation_wait (operation);
    
    if (seahorse_operation_is_successful (operation)) {
        keylist = (GList*)seahorse_operation_get_result (operation);
        keys = g_list_length (keylist);
    } else {
        seahorse_operation_steal_error (operation, err);
        keys = -1;
    }
    
    g_object_unref (operation);
    gpgmex_data_release (data);

    return keys;    
}

/**
 * seahorse_op_import_file:
 * @sksrc: #SeahorseKeySource to import into
 * @path: Path of file to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in the file @path, saving any error in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_file (SeahorsePGPSource *psrc, const gchar *path, GError **err)
{
    gpgme_data_t data;
  
    /* new data from file */
    data = seahorse_vfs_data_create (path, SEAHORSE_VFS_READ, err);
    if (!data) 
        return -1;
    
    return import_data (psrc, data, err);
}

/**
 * seahorse_op_import_text:
 * @sksrc: #SeahorseKeySource to import into 
 * @text: Text to import
 * @err: Optional error value
 *
 * Tries to import any keys contained in @text, saving any errors in @err.
 *
 * Returns: Number of keys imported or -1 if import fails
 **/
gint
seahorse_op_import_text (SeahorsePGPSource *psrc, const gchar *text, GError **err)
{
    gpgme_data_t data;
    
    g_return_val_if_fail (text != NULL, 0);
         
    /* new data from text */
    data = gpgmex_data_new_from_mem (text, strlen (text), TRUE);
    return import_data (psrc, data, err);
}

