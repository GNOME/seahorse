/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#include <gnome.h>
#include <gpgme.h>

#include "seahorse-ops-text.h"
#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-ops-data.h"

/* Reads data into string, releases data when finished. */
static gboolean
read_string (GpgmeData data, GString *string)
{
        gint size = 128;
        gchar *buffer;
        guint nread = 0;

	if (string == NULL)
		string = g_string_new ("");
	else
		string = g_string_erase (string, 0, -1);
	
	buffer = g_new (gchar, size);
	
	while (gpgme_data_read (data, buffer, size, &nread) == GPGME_No_Error)
		string = g_string_append_len (string, buffer, nread);
	
	gpgme_data_release (data);
	
	return TRUE;
}

/**
 * seahorse_ops_text_import:
 * @sctx: #SeahorseContext
 * @text: Text to import
 *
 * Imports @text using seahorse_ops_data_import().
 * seahorse_context_show_status() will be called upon completion.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_text_import (SeahorseContext *sctx, const gchar *text)
{
	GpgmeData data;
	
	return ((gpgme_data_new_from_mem (&data, text, strlen (text), TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_import (sctx, data));
}

/**
 * seahorse_ops_text_export:
 * @sctx: #SeahorseContext with ascii armor enabled
 * @string: Will contain exported @
 **/
gboolean
seahorse_ops_text_export (SeahorseContext *sctx, GString *string, const SeahorseKey *skey)
{	
	GpgmeData data;
	gboolean success;
	
	success = (gpgme_get_armor (sctx->ctx) &&
		seahorse_ops_data_export (sctx, &data, skey) &&
		read_string (data, string));
	
	if (!success)
		gpgme_data_release (data);
	
	seahorse_context_show_status (sctx, _("Export Text"), success);
	return success;
}

gboolean
seahorse_ops_text_export_multiple (SeahorseContext *sctx, GString *string, GpgmeRecipients recips)
{
	GpgmeData data;
	gboolean success;
	
	g_return_val_if_fail (sctx != NULL, FALSE);
	
	success = (seahorse_ops_data_export_multiple (sctx, &data, recips) &&
		read_string (data, string));
	
	seahorse_context_show_status (sctx, _("Export Text"), success);
	return success;
}

/* Common sign function */
static gboolean
text_sign_mode (SeahorseContext *sctx, GString *string, GpgmeSigMode mode, gchar *op)
{
	GpgmeData sig;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_mem (&plain, string->str, strlen (string->str), TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_sign (sctx, plain, &sig, mode) &&
		read_string (sig, string));
	
	seahorse_context_show_status (sctx, op, success);
	return success;
}

gboolean
seahorse_ops_text_sign (SeahorseContext *sctx, GString *string)
{
	return text_sign_mode (sctx, string, GPGME_SIG_MODE_NORMAL, _("Sign"));
}

gboolean
seahorse_ops_text_clear_sign (SeahorseContext *sctx, GString *string)
{
	return text_sign_mode (sctx, string, GPGME_SIG_MODE_CLEAR, _("Clear Sign"));
}

gboolean
seahorse_ops_text_encrypt (SeahorseContext *sctx, GString *string, GpgmeRecipients recips)
{
	GpgmeData cipher;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_mem (&plain, string->str, strlen (string->str), TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_encrypt (sctx, recips, plain, &cipher) &&
		read_string (cipher, string));
	
	seahorse_context_show_status (sctx, _("Encrypt"), success);
	return success;
}

gboolean
seahorse_ops_text_decrypt (SeahorseContext *sctx, GString *string)
{
	GpgmeData cipher;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_mem (&cipher, string->str, strlen (string->str), TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_decrypt (sctx, cipher, &plain) &&
		read_string (plain, string));
	
	seahorse_context_show_status (sctx, _("Decrypt"), success);
	return success;
}

/* Returns TRUE if status is good or have multiple stati */
static gboolean
stat_good (GpgmeSigStat status)
{
	return (status == GPGME_SIG_STAT_GOOD || status == GPGME_SIG_STAT_GOOD_EXPKEY || status == GPGME_SIG_STAT_DIFF);
}

gboolean
seahorse_ops_text_verify (SeahorseContext *sctx, GString *string, GpgmeSigStat *status)
{
	GpgmeData plain;
	GpgmeData sig;
	gboolean success;
	
	success = ((gpgme_data_new (&plain) == GPGME_No_Error) &&
		(gpgme_data_new_from_mem (&sig, string->str, strlen (string->str), TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_verify (sctx, sig, plain, status) &&
		stat_good (*status) &&
		read_string (plain, string));
	
	seahorse_context_show_status (sctx, _("Verify Signature"), success);
	return success;
}

gboolean
seahorse_ops_text_decrypt_verify (SeahorseContext *sctx, GString *string, GpgmeSigStat *status)
{
	GpgmeData cipher;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_mem (&cipher, string->str, strlen (string->str), TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_decrypt_verify (sctx, cipher, &plain, status) &&
		stat_good (*status) &&
		read_string (plain, string));
	
	seahorse_context_show_status (sctx, _("Decrypt & Verify"), success);
	return success;
}
