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

#include "seahorse-ops-data.h"
#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-ops-key.h"

gboolean
seahorse_ops_data_import (SeahorseContext *sctx, const GpgmeData data)
{
	gboolean success;
	gint keys = 0;
	gchar *message;
	
	success = (gpgme_op_import_ext (sctx->ctx, data, &keys) == GPGME_No_Error);
	gpgme_data_release (data);
	
	if (success)
		seahorse_context_key_added (sctx);
	
	message = g_strdup_printf (_("Imported %d Keys: "), keys);
	
	seahorse_context_show_status (sctx, message, success);
	return success;
}

gboolean
seahorse_ops_data_export (const SeahorseContext *sctx, GpgmeData *data, const SeahorseKey *skey)
{
	GpgmeRecipients recips;
	
	return ((gpgme_recipients_new (&recips) == GPGME_No_Error) &&
		seahorse_ops_key_recips_add (recips, skey) &&
		seahorse_ops_data_export_multiple (sctx, data, recips));
}

gboolean
seahorse_ops_data_export_multiple (const SeahorseContext *sctx, GpgmeData *data, GpgmeRecipients recips)
{
	gboolean success;
	
	success = (recips != NULL && (gpgme_data_new (data) == GPGME_No_Error) &&
		(gpgme_op_export (sctx->ctx, recips, *data) == GPGME_No_Error) &&
		(gpgme_data_rewind (*data) == GPGME_No_Error));
	
	gpgme_recipients_release (recips);
	
	return success;
}

gboolean
seahorse_ops_data_sign (const SeahorseContext *sctx, GpgmeData plain, GpgmeData *sig, GpgmeSigMode mode)
{
	gboolean success;
	
	success = ((gpgme_data_new (sig) == GPGME_No_Error) &&
		(gpgme_op_sign (sctx->ctx, plain, *sig, mode) == GPGME_No_Error));
	
	gpgme_data_release (plain);
	
	return success;
}

gboolean
seahorse_ops_data_encrypt (const SeahorseContext *sctx, GpgmeRecipients recips, GpgmeData plain, GpgmeData *cipher)
{
	gboolean success;
	
	success = (recips != NULL && (gpgme_data_new (cipher) == GPGME_No_Error) &&
		gpgme_op_encrypt (sctx->ctx, recips, plain, *cipher) == GPGME_No_Error);
	
	gpgme_recipients_release (recips);
	gpgme_data_release (plain);
	
	return success;
}

gboolean
seahorse_ops_data_decrypt (const SeahorseContext *sctx, GpgmeData cipher, GpgmeData *plain)
{
	gboolean success;
	
	success = ((gpgme_data_new (plain) == GPGME_No_Error) &&
		(gpgme_op_decrypt (sctx->ctx, cipher, *plain) == GPGME_No_Error));
	
	gpgme_data_release (cipher);
	
	return success;
}

gboolean
seahorse_ops_data_verify (const SeahorseContext *sctx, GpgmeData sig, GpgmeData plain, GpgmeSigStat *status)
{
	gboolean success;
	
	success = (gpgme_op_verify (sctx->ctx, sig, plain, status) == GPGME_No_Error);
	
	gpgme_data_release (sig);
	
	return success;
}

gboolean
seahorse_ops_data_decrypt_verify (const SeahorseContext *sctx, GpgmeData cipher, GpgmeData *plain, GpgmeSigStat *status)
{
	gboolean success;
	
	success = ((gpgme_data_new (plain) == GPGME_No_Error) &&
		(gpgme_op_decrypt_verify (sctx->ctx, cipher, *plain, status) == GPGME_No_Error));
	
	gpgme_data_release (cipher);
	
	return success;
}
