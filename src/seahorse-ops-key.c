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
#include <time.h>

#include "seahorse-ops-key.h"
#include "seahorse-context.h"
#include "seahorse-key.h"
#include "seahorse-util.h"

#define PROMPT "keyedit.prompt"
#define QUIT "quit"
#define SAVE "keyedit.save.okay"
#define YES "Y"

gboolean
seahorse_ops_key_delete (SeahorseContext *sctx, SeahorseKey *skey)
{
	gboolean success;
	
	success = (gpgme_op_delete (sctx->ctx, skey->key, TRUE) == GPGME_No_Error);
	
	if (success)
		seahorse_key_destroy (skey);
	
	seahorse_context_show_status (sctx, _("Delete Key"), success);
	return success;
}

gboolean
seahorse_ops_key_generate (SeahorseContext *sctx, const gchar *name,
			   const gchar *email, const gchar *comment,
			   const gchar *passphrase, const SeahorseKeyType type,
			   const guint length, const time_t expires)
{
        gchar *common;
        gchar *key_type;
        gchar *start;
        gchar *parms;
        gboolean success;
	const gchar *expires_date;
	
	/* Texts cannot be empty */
	g_assert (!g_str_equal (name, "") && !g_str_equal (email, "") &&
		  !g_str_equal (comment, "") && !g_str_equal (passphrase, ""));
	
	if (expires != 0)
		expires_date = seahorse_util_get_date_string (expires);
	else
		expires_date = "0";
	
	/* Common xml */
	common = g_strdup_printf ("Name-Real: %s\nName-Comment: %s\nName-Email: %s\n"
		"Expire-Date: %s\nPassphrase: %s\n</GnupgKeyParms>", name, comment, email,
		expires_date, passphrase);
	
	if (type == RSA)
		key_type = "Key-Type: RSA";
	else
		key_type = "Key-Type: DSA";
	
	/* More common xml */
	start = g_strdup_printf ("<GnupgKeyParms format=\"internal\">\n%s\nKey-Length: ", key_type);
	
	/* Subkey xml */
	if (type == DSA_ELGAMAL)
		parms = g_strdup_printf ("%s%d\nSubkey-Type: ELG-E\nSubkey-Length: %d\n%s",
					 start, DSA_MAX, length, common);
	else
		parms = g_strdup_printf ("%s%d\n%s", start, length, common);
	
	success = (gpgme_op_genkey (sctx->ctx, parms, NULL, NULL) == GPGME_No_Error);
	
	if (success)
		seahorse_context_key_added (sctx);
	
	seahorse_context_show_status (sctx, _("Generate Key"), success);
	
	/* Free xmls */
	g_free (parms);
	g_free (start);
	g_free (common);
	
	return success;
}

gboolean
seahorse_ops_key_export_server (SeahorseContext *sctx, SeahorseKey *skey, const gchar *server)
{
	return FALSE;
}

gboolean
seahorse_ops_key_import_server (SeahorseContext *sctx, const gchar *keyid, const gchar *server)
{
	return FALSE;
}

gboolean
seahorse_ops_key_recips_add (GpgmeRecipients recips, const SeahorseKey *skey)
{
	const gchar *name;
	gboolean success;
	
	g_return_val_if_fail (recips != NULL, FALSE);
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), FALSE);
	
	name = gpgme_key_get_string_attr (skey->key, GPGME_ATTR_KEYID, NULL, 0);
	
	//FULL seems to be minimum acceptable
	success = (gpgme_recipients_add_name_with_validity (recips, name, GPGME_VALIDITY_FULL) == GPGME_No_Error);
	
	return success;
}

/* Main key edit setup, structure, and a good deal of method content borrowed from gpa */

/* Edit action function */
typedef GpgmeError	(*SeahorseEditAction) 	(guint			state,
						 gpointer 		data,
						 const gchar		**result);
/* Edit transit function */
typedef guint		(*SeahorseEditTransit)	(guint			current_state,
						 GpgmeStatusCode	status,
						 const gchar		*args,
						 gpointer		data,
						 GpgmeError		*err);
/* Edit parameters */
typedef struct
{
	guint 			state;
	GpgmeError		err;
	SeahorseEditAction	action;
	SeahorseEditTransit	transit;
	gpointer		data;
	
} SeahorseEditParm;

/* Creates new edit parameters with defaults */
static SeahorseEditParm*
seahorse_edit_parm_new (guint state, SeahorseEditAction action, SeahorseEditTransit transit, gpointer data)
{
	SeahorseEditParm *parms;
	
	parms = g_new0 (SeahorseEditParm, 1);
	parms->state = state;
	parms->err = GPGME_No_Error;
	parms->action = action;
	parms->transit = transit;
	parms->data = data;
	
	return parms;
}

/* Edit callback for gpgme */
static GpgmeError
seahorse_ops_key_edit (gpointer data, GpgmeStatusCode status, const gchar *args, const gchar **result)
{
	SeahorseEditParm *parms = (SeahorseEditParm*)data;
	
	/* Ignore these status lines, as they don't require any response */
	if (status == GPGME_STATUS_EOF ||
	    status == GPGME_STATUS_GOT_IT ||
	    status == GPGME_STATUS_NEED_PASSPHRASE ||
	    status == GPGME_STATUS_GOOD_PASSPHRASE ||
	    status == GPGME_STATUS_BAD_PASSPHRASE ||
	    status == GPGME_STATUS_USERID_HINT ||
	    status == GPGME_STATUS_SIGEXPIRED ||
	    status == GPGME_STATUS_KEYEXPIRED)
		return parms->err;

	/* Choose the next state based on the current one and the input */
	parms->state = parms->transit (parms->state, status, args, parms->data, &parms->err);
	
	/* Choose the action based on the state */
	if (parms->err == GPGME_No_Error)
		parms->err = parms->action (parms->state, parms->data, result);
	
	return parms->err;
}

/* Common edit operations */
static gboolean
edit_key (SeahorseContext *sctx, SeahorseKey *skey, SeahorseEditParm *parms, const gchar *op, SeahorseKeyChange change)
{
	GpgmeData out = NULL;
	gboolean success;
	
	success = ((gpgme_data_new (&out) == GPGME_No_Error) &&
		(gpgme_op_edit (sctx->ctx, skey->key, seahorse_ops_key_edit, parms, out) == GPGME_No_Error));
	
	if (out != NULL)
		gpgme_data_release (out);
	if (success)
		seahorse_key_changed (skey, change);
	
	seahorse_context_show_status (sctx, op, success);
	return success;
}

/* Edit owner trust states */
typedef enum
{
	TRUST_START,
	TRUST_COMMAND,
	TRUST_VALUE,
	TRUST_REALLY_ULTIMATE,
	TRUST_QUIT,
	TRUST_SAVE,
	TRUST_ERROR
} TrustState;

/* Edit owner trust actions */
static GpgmeError
edit_trust_action (guint state, gpointer data, const gchar **result)
{
	const gchar *trust = data;
	
	switch (state) {
		/* Start the operation */
		case TRUST_COMMAND:
			*result = "trust";
			break;
		/* Send the new trust date */
		case TRUST_VALUE:
			*result = trust;
			break;
		/* Really set to ultimate trust */
		case TRUST_REALLY_ULTIMATE:
			*result = YES;
			break;
		/* End the operation */
		case TRUST_QUIT:
			*result = QUIT;
			break;
		/* Save */
		case TRUST_SAVE:
			*result = YES;
			break;
		/* Special state: an error ocurred. Do nothing until we can quit */
		case TRUST_ERROR:
			break;
		/* Can't happen */
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

/* Edit owner trust transits */
static guint
edit_trust_transit (guint current_state, GpgmeStatusCode status, const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		case TRUST_START:
			/* Need to enter command */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_COMMAND;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case TRUST_COMMAND:
			/* Need to enter value */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "edit_ownertrust.value"))
				next_state = TRUST_VALUE;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case TRUST_VALUE:
			/* Entered value, quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_QUIT;
			/* Confirm ultimate trust */
			else if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "edit_ownertrust.set_ultimate.okay"))
				next_state = TRUST_REALLY_ULTIMATE;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case TRUST_REALLY_ULTIMATE:
			/* Confirmed, quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_QUIT;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case TRUST_QUIT:
			/* Quit, save changes */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = TRUST_SAVE;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case TRUST_ERROR:
			/* Go to quit operation state */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_QUIT;
			else
				next_state = TRUST_ERROR;
			break;
		default:
			next_state = TRUST_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

gboolean
seahorse_ops_key_set_trust (SeahorseContext *sctx, SeahorseKey *skey, GpgmeValidity trust)
{
	SeahorseEditParm *parms;
	gchar *trust_strings[] = {"1", "2", "3", "4", "5"};
	
	g_return_val_if_fail (seahorse_validity_get_index (trust) != seahorse_validity_get_index (
		gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_OTRUST, NULL, 0)), FALSE);
	
	parms = seahorse_edit_parm_new (TRUST_START, edit_trust_action, edit_trust_transit,
		trust_strings[seahorse_validity_get_index (trust)]);
	
	return edit_key (sctx, skey, parms, _("Change Trust"), SKEY_CHANGE_TRUST);
}

/* Edit primary expiration states */
typedef enum
{
	EXPIRE_START,
	EXPIRE_COMMAND,
	EXPIRE_DATE,
	EXPIRE_QUIT,
	EXPIRE_SAVE,
	EXPIRE_ERROR
} ExpireState;

/* Edit primary expiration actions */
static GpgmeError
edit_expire_action (guint state, gpointer data, const gchar **result)
{
	const gchar *date = data;
  
	switch (state) {
		/* Start the operation */
		case EXPIRE_COMMAND:
			*result = "expire";
			break;
		/* Send the new expire date */
		case EXPIRE_DATE:
			*result = date;
			break;
		/* End the operation */
		case EXPIRE_QUIT:
			*result = QUIT;
			break;
		/* Save */
		case EXPIRE_SAVE:
			*result = YES;
			break;
		/* Special state: an error ocurred. Do nothing until we can quit */
		case EXPIRE_ERROR:
			break;
		/* Can't happen */
		default:
			return GPGME_General_Error;
	}
	return GPGME_No_Error;
}

/* Edit primary expiration transits */
static guint
edit_expire_transit (guint current_state, GpgmeStatusCode status, const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
 
	switch (current_state) {
		case EXPIRE_START:
			/* Need to enter command */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_COMMAND;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case EXPIRE_COMMAND:
			/* Need to enter date */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.valid"))
				next_state = EXPIRE_DATE;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case EXPIRE_DATE:
			/* Done, quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_QUIT;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case EXPIRE_QUIT:
			/* Quit, save */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = EXPIRE_SAVE;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case EXPIRE_ERROR:
			/* Go to quit operation state */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_QUIT;
			else
				next_state = EXPIRE_ERROR;
			break;
		default:
			next_state = EXPIRE_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	return next_state;
}

gboolean
seahorse_ops_key_set_expires (SeahorseContext *sctx, SeahorseKey *skey, time_t expires)
{
	const gchar *date;
	SeahorseEditParm *parms;
	
	if (expires)
		date = seahorse_util_get_date_string (expires);
	else
		date = "0";
	
	g_return_val_if_fail (!g_str_equal (date, seahorse_util_get_date_string (
		gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_EXPIRE, NULL, 0))), FALSE);
	
	parms = seahorse_edit_parm_new (EXPIRE_START, edit_expire_action, edit_expire_transit, (gchar*)date);
	
	return edit_key (sctx, skey, parms, _("Change Expiration"), SKEY_CHANGE_EXPIRE);
}

/* Edit disable/enable states */
typedef enum {
	DISABLE_START,
	DISABLE_COMMAND,
	DISABLE_QUIT,
	DISABLE_ERROR
} DisableState;

/* Edit disable/enable actions */
static GpgmeError
edit_disable_action (guint state, gpointer data, const gchar **result)
{
	const gchar *command = data;
	
	switch (state) {
		/* disable / enable */
		case DISABLE_COMMAND:
			*result = command;
			break;
		/* Quit */
		case DISABLE_QUIT:
			*result = QUIT;
			break;
		default:
			break;
	}
	
	return GPGME_No_Error;
}

/* Edit disable/enable transits */
static guint
edit_disable_transit (guint current_state, GpgmeStatusCode status, const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		case DISABLE_START:
			/* Need command */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_COMMAND;
			else {
				next_state = DISABLE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case DISABLE_COMMAND:
			/* Done, quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_QUIT;
			else {
				next_state = DISABLE_ERROR;
				*err = GPGME_General_Error;
			}
		case DISABLE_ERROR:
			/* Go to quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_QUIT;
			else
				next_state = DISABLE_ERROR;
			break;
		default:
			next_state = DISABLE_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

gboolean
seahorse_ops_key_set_disabled (SeahorseContext *sctx, SeahorseKey *skey, gboolean disabled)
{
	gchar *command;
	SeahorseEditParm *parms;
	gchar *op;
	
	g_return_val_if_fail (disabled != gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_KEY_DISABLED, NULL, 0), FALSE);
	
	if (disabled) {
		command = "disable";
		op = _("Disable Key");
	}
	else {
		command = "enable";
		op = _("Enable Key");
	}
	
	parms = seahorse_edit_parm_new (DISABLE_START, edit_disable_action, edit_disable_transit, command);
	
	return edit_key (sctx, skey, parms, op, SKEY_CHANGE_DISABLE);
}

/* Passphrase change states.  These are all that are necessary if gpgme passphrase callback is set. */
typedef enum {
	PASS_START,
	PASS_COMMAND,
	PASS_QUIT,
	PASS_SAVE,
	PASS_ERROR
} PassState;

/* Passphrase change actions */
static GpgmeError
edit_pass_action (guint state, gpointer data, const gchar **result)
{
	switch (state) {
		case PASS_COMMAND:
			*result = "passwd";
			break;
		case PASS_QUIT:
			*result = QUIT;
			break;
		case PASS_SAVE:
			*result = YES;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;}

/* Passphrase change transits */
static guint
edit_pass_transit (guint current_state, GpgmeStatusCode status, const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		case PASS_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = PASS_COMMAND;
			else {
				next_state = PASS_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* After entering command, gpgme will take care of rest with passphrase callbacks */
		case PASS_COMMAND:
			next_state = PASS_QUIT;
			break;
		case PASS_QUIT:
			/* Quit, save */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = PASS_SAVE;
			else {
				next_state = PASS_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case PASS_ERROR:
			/* Go to quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = PASS_QUIT;
			else
				next_state = PASS_ERROR;
			break;
		default:
			next_state = PASS_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

gboolean
seahorse_ops_key_change_pass (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseEditParm *parms;
	
	parms = seahorse_edit_parm_new (PASS_START, edit_pass_action, edit_pass_transit, NULL);
	
	return edit_key (sctx, skey, parms, _("Passphrase Change"), SKEY_CHANGE_PASS);
}
