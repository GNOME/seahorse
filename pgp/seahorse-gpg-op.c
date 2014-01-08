/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

#include "config.h"
 
#include <glib.h>
#include <gpgme.h>
#include <string.h>

#include "pgp/seahorse-gpg-op.h"
#include "pgp/seahorse-gpgme.h"

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
seahorse_gpg_op_export_secret (gpgme_ctx_t ctx,
                               const gchar **patterns,
                               gpgme_data_t keydata)
{
	gchar *output = NULL;
	gpgme_error_t gerr;
	gchar *args;
	gsize i;

	g_return_val_if_fail (patterns != NULL, GPG_E (GPG_ERR_INV_VALUE));

	for (i = 0; patterns[i] != NULL; i++) {
		args = g_strdup_printf ("--armor --export-secret-key '%s'", patterns[i]);
		gerr = execute_gpg_command (ctx, args, &output, NULL);
		g_free (args);

		if (!GPG_IS_OK (gerr))
			return gerr;

		if (gpgme_data_write (keydata, output, strlen (output)) == -1)
			return GPG_E (GPG_ERR_GENERAL);

		g_free (output);
	}

	return GPG_OK;
}

gpgme_error_t 
seahorse_gpg_op_num_uids (gpgme_ctx_t ctx, const char *pattern, guint *number)
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
