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
#include <glib.h>

#include "seahorse-ops-file.h"
#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-ops-data.h"

#define ASC ".asc"
#define GPG ".gpg"
#define SIG ".sig"

/* Returns new file name with appropriate suffix */
static gchar*
add_suffix (const gchar *file, SeahorseFileType type, SeahorseContext *sctx)
{
	if (gpgme_get_armor (sctx->ctx))
		return g_strdup_printf ("%s%s", file, ASC);
	else if (type == SIG_FILE)
		return g_strdup_printf ("%s%s", file, SIG);
	else
		return g_strdup_printf ("%s%s", file, GPG);
}

/* Returns new file name with suffix removed */
static gchar*
del_suffix (const gchar *file, SeahorseFileType type)
{
	if (seahorse_ops_file_check_suffix (file, type))
		return g_strndup (file, strlen (file) - 4);
	else
		return NULL;
}

/* Writes data to a file, borrowed from gpa */
static gboolean
write_data (GpgmeData data, const gchar *file)
{
	FILE *fp;
	gchar *buffer;
	gint nread;
	
	fp = fopen (file, "a");
	if (fp == NULL)
		return FALSE;
	
	buffer = g_new0 (gchar, 128);
	
	while (gpgme_data_read (data, buffer, sizeof (buffer), &nread) == GPGME_No_Error)
		fwrite (buffer, nread, 1, fp);
	
	fflush (fp);
	fclose (fp);
	gpgme_data_release (data);
	
	return TRUE;
}

gboolean
seahorse_ops_file_check_suffix (const gchar *file, SeahorseFileType type)
{
	gchar *suffix;
	
	if (type == SIG_FILE)
		suffix = SIG;
	else
		suffix = GPG;
	
	return (g_pattern_match_simple (g_strdup_printf ("*%s", ASC), file) ||
		g_pattern_match_simple (g_strdup_printf ("*%s", suffix), file));
}

gboolean
seahorse_ops_file_import (SeahorseContext *sctx, const gchar *file)
{
	GpgmeData data;
	gboolean success;
	
	success = (gpgme_data_new_from_file (&data, file, TRUE) == GPGME_No_Error);
	if (success)
		success = seahorse_ops_data_import (sctx, data);
	else
		seahorse_context_show_status (sctx, _("Bad File, Import"), FALSE);
	
	return success;
}

gboolean
seahorse_ops_file_export (SeahorseContext *sctx, const gchar *file, const SeahorseKey *skey)
{
	GpgmeData data;
	gboolean success;
	
	success = (seahorse_ops_data_export (sctx, &data, skey) &&
		write_data (data, file));
	
	seahorse_context_show_status (sctx, _("Export File"), success);
	return success;
}

gboolean
seahorse_ops_file_export_multiple (SeahorseContext *sctx, const gchar *file, GpgmeRecipients recips)
{
	GpgmeData data;
	gboolean success;
	
	success = (seahorse_ops_data_export_multiple (sctx, &data, recips) &&
		write_data (data, file));
	
	seahorse_context_show_status (sctx, _("Export File"), success);
	return success;
}

gboolean
seahorse_ops_file_sign (SeahorseContext *sctx, const gchar *file)
{
	GpgmeData plain;
	GpgmeData sig;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&plain, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_sign (sctx, plain, &sig, GPGME_SIG_MODE_DETACH) &&
		write_data (sig, add_suffix (file, SIG_FILE, sctx)));
	
	seahorse_context_show_status (sctx, _("Detached Signature"), success);
	return success;
}

gboolean
seahorse_ops_file_encrypt (SeahorseContext *sctx, const gchar *file, GpgmeRecipients recips)
{
	GpgmeData plain;
	GpgmeData cipher;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&plain, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_encrypt (sctx, recips, plain, &cipher) &&
		write_data (cipher, add_suffix (file, CRYPT_FILE, sctx)));
	
	seahorse_context_show_status (sctx, _("Encrypt File"), success);
	return success;
}

gboolean
seahorse_ops_file_decrypt (SeahorseContext *sctx, const gchar *file)
{
	GpgmeData cipher;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&cipher, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_decrypt (sctx, cipher, &plain) &&
		write_data (plain, del_suffix (file, CRYPT_FILE)));

	seahorse_context_show_status (sctx, _("Decrypt File"), success);
	return success;
}

gboolean
seahorse_ops_file_verify (SeahorseContext *sctx, const gchar *file, GpgmeSigStat *status)
{
	GpgmeData sig;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&plain, del_suffix (file, SIG_FILE), TRUE) == GPGME_No_Error) &&
		(gpgme_data_new_from_file (&sig, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_verify (sctx, sig, plain, status));
	
	gpgme_data_release (plain);
	
	seahorse_context_show_status (sctx, _("Verify Signature File"), success);
	return success;
}

gboolean
seahorse_ops_file_decrypt_verify (SeahorseContext *sctx, const gchar *file, GpgmeSigStat *status)
{
	GpgmeData cipher;
	GpgmeData plain;
	gboolean success;
	
	success = ((gpgme_data_new_from_file (&cipher, file, TRUE) == GPGME_No_Error) &&
		seahorse_ops_data_decrypt_verify (sctx, cipher, &plain, status) &&
		write_data (plain, del_suffix (file, CRYPT_FILE)));
	
	seahorse_context_show_status (sctx, _("Decrypt & Verify File"), success);
	return success;
}

