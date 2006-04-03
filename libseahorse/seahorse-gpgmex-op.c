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
 
#include <glib.h>
#include <gpgme.h>
#include <string.h>

#include "seahorse-gpgmex.h"

static gpgme_error_t
execute_gpg_command (gpgme_ctx_t ctx, const gchar *args, gchar **std_out, 
                     gchar **std_err)
{
    gpgme_engine_info_t engine;
    gpgme_error_t gerr;
    GError *err = NULL;
    gint status;
    gchar *cmd;
    
    gerr = gpgme_get_engine_info (&engine);
    g_return_val_if_fail (GPG_IS_OK (gerr), gerr);
    
    /* Look for the OpenPGP engine */
    while (engine && engine->protocol != GPGME_PROTOCOL_OpenPGP)
        engine = engine->next;
    
    g_return_val_if_fail (engine != NULL && engine->file_name, GPG_E (GPG_ERR_INV_ENGINE));
    
    gerr = GPG_OK;
    
    cmd = g_strdup_printf ("%s --batch %s", engine->file_name, args);
    if (!g_spawn_command_line_sync (cmd, std_out, std_err, &status, &err) || 
        status != 0) {
        gerr = GPG_E (GPG_ERR_GENERAL);
        if(err != NULL)
            g_error_free (err);
    }

    g_free (cmd);    
    return gerr;
}

gpgme_error_t        
gpgmex_op_export_secret  (gpgme_ctx_t ctx, 
                          const char *pattern,
                          gpgme_data_t keydata)
{
    gchar *output = NULL;
    gpgme_error_t err;
    gboolean armor;
    gchar *args;
    
    g_return_val_if_fail (pattern != NULL, GPG_E (GPG_ERR_INV_VALUE));
    /* TODO: Use the context's arguments for armor */
    armor = gpgme_get_armor (ctx) ? TRUE : FALSE;
    args = g_strdup_printf ("%s --export-secret-key '%s'", 
                            armor ? "--armor" : "",
                            pattern);
    
    err = execute_gpg_command (ctx, args, &output, NULL);
    g_free (args);
    
    if (!GPG_IS_OK (err))
        return err;
    
    if (gpgme_data_write (keydata, output, strlen (output)) == -1)
        return GPG_E (GPG_ERR_GENERAL);
    
    g_free (output);
    return GPG_OK;
}

gpgme_error_t 
gpgmex_op_num_uids (gpgme_ctx_t ctx, const char *pattern, guint *number)
{
	 gchar *output = NULL;
    gpgme_error_t err;
    gchar *args;
    gchar *found = NULL;
    
    g_return_val_if_fail (pattern != NULL, GPG_E (GPG_ERR_INV_VALUE));
    args = g_strdup_printf ("--list-keys '%s'", pattern);
    
    err = execute_gpg_command (ctx, args, &output, NULL);
    g_free (args);
    
    if (!GPG_IS_OK (err))
        return err;
    
    found = output;
	while ((found = strstr(found, "uid")) != NULL) {
		*number = *number + 1;
		found += 3;
	}

	if ((GPG_MAJOR == 1) && (GPG_MINOR == 2))
		*number = *number + 1;
     
    g_free (output);
    return GPG_OK;
}
