/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

/**
 * seahorse_ops_key_delete:
 * @sctx: Current context
 * @skey: Key to delete
 *
 * Deletes key, removing it from the context and emitting the key's destroy
 * signal.
 *
 * Returns: %TRUE if key is deleted, %FALSE otherwise
 **/
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

/**
 * seahorse_ops_key_generate:
 * @sctx: The current context
 * @name: Name for the new key
 * @email: Email address for new key
 * @comment: New key comment
 * @passphrase: Passphrase for new key
 * @type: #SeahorseKeyType, must be #DSA_ELGAMAL, #DSA, or #RSA_SIGN
 * @length: Length of key, must be within #SeahorseKeyLength restraints,
 * depending on @type
 * @expires: Expiration date, 0 is never
 *
 * Generates a new key.  @sctx will add the new key and emit the key added
 * signal.
 *
 * Returns: %TRUE if operation is successful, %FALSE otherwise.
 **/
gboolean
seahorse_ops_key_generate (SeahorseContext *sctx, const gchar *name,
			   const gchar *email, const gchar *comment,
			   const gchar *passphrase, const SeahorseKeyType type,
			   const guint length, const time_t expires)
{
        gchar *common, *key_type, *start, *parms;
        gboolean success;
	const gchar *expires_date;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (name != NULL && !g_str_equal (name, ""), FALSE);
	g_return_val_if_fail (email != NULL && seahorse_ops_key_check_email (email), FALSE);
	g_return_val_if_fail (comment != NULL && !g_str_equal (comment, ""), FALSE);
	g_return_val_if_fail (passphrase != NULL && !g_str_equal (passphrase, ""), FALSE);
	
	/* Check lengths for each type */
	switch (type) {
		case DSA_ELGAMAL:
			g_return_val_if_fail (length >= ELGAMAL_MIN && length <= LENGTH_MAX, FALSE);
			break;
		case DSA:
			g_return_val_if_fail (length >= DSA_MIN && length <= DSA_MAX, FALSE);
			break;
		case RSA_SIGN:
			g_return_val_if_fail (length >= RSA_MIN && length <= LENGTH_MAX, FALSE);
			break;
		default:
			g_return_val_if_reached (FALSE);
			break;
	}
	
	if (expires != 0)
		expires_date = seahorse_util_get_date_string (expires);
	else
		expires_date = "0";
	
	/* Common xml */
	common = g_strdup_printf ("Name-Real: %s\nName-Comment: %s\nName-Email: %s\n"
		"Expire-Date: %s\nPassphrase: %s\n</GnupgKeyParms>", name, comment, email,
		expires_date, passphrase);
	
	if (type == RSA_SIGN)
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

/**
 * seahorse_ops_key_recips_add:
 * @recips: Current recipients list
 * @skey: Key to add to @recips
 *
 * Adds @skey to @recips with full validity.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_key_recips_add (GpgmeRecipients recips, const SeahorseKey *skey)
{
	const gchar *name;
	gboolean success;
	
	g_return_val_if_fail (recips != NULL, FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	
	//FULL seems to be minimum acceptable
	success = (gpgme_recipients_add_name_with_validity (recips,
		seahorse_key_get_keyid (skey, 0), GPGME_VALIDITY_FULL) == GPGME_No_Error);
	
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
seahorse_edit_parm_new (guint state, SeahorseEditAction action,
			SeahorseEditTransit transit, gpointer data)
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
seahorse_ops_key_edit (gpointer data, GpgmeStatusCode status,
		       const gchar *args, const gchar **result)
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
edit_key (SeahorseContext *sctx, SeahorseKey *skey, SeahorseEditParm *parms,
	  const gchar *op, SeahorseKeyChange change)
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
edit_trust_transit (guint current_state, GpgmeStatusCode status,
		    const gchar *args, gpointer data, GpgmeError *err)
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

/**
 * seahorse_ops_key_set_trust:
 * @sctx: Current context
 * @skey: Key to change
 * @trust: New trust value for @skey
 *
 * Sets @skey trust to @trust.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_key_set_trust (SeahorseContext *sctx, SeahorseKey *skey, GpgmeValidity trust)
{
	SeahorseEditParm *parms;
	static gchar *trust_strings[] = {"1", "2", "3", "4", "5"};
	guint index;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	
	index = seahorse_validity_get_index (trust);
	/* Make sure _changing_ trust */
	g_return_val_if_fail (index != seahorse_validity_get_index (
		gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_OTRUST, NULL, 0)), FALSE);
	
	parms = seahorse_edit_parm_new (TRUST_START, edit_trust_action,
		edit_trust_transit, trust_strings[index]);
	
	return edit_key (sctx, skey, parms, _("Change Trust"), SKEY_CHANGE_TRUST);
}

typedef struct
{
	guint	index;
	time_t	expires;
} ExpireParm;

/* Edit primary expiration states */
typedef enum
{
	EXPIRE_START,
	EXPIRE_SELECT,
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
	ExpireParm *parm = (ExpireParm*)data;
  
	switch (state) {
		case EXPIRE_SELECT:
			*result = g_strdup_printf ("key %d", parm->index);
			break;
		/* Start the operation */
		case EXPIRE_COMMAND:
			*result = "expire";
			break;
		/* Send the new expire date */
		case EXPIRE_DATE:
			*result = (parm->expires) ?
				seahorse_util_get_date_string (parm->expires) : "0";
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
edit_expire_transit (guint current_state, GpgmeStatusCode status,
		     const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
 
	switch (current_state) {
		case EXPIRE_START:
			/* Need to enter command */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_SELECT;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case EXPIRE_SELECT:
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

/**
 * seahorse_ops_key_set_expires:
 * @sctx: Current context
 * @skey: Key to change
 * @expires: New expiration time for @skey. 0 is never.
 *
 * Changes expiration date of @skey to @expires.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_key_set_expires (SeahorseContext *sctx, SeahorseKey *skey,
			      guint index, time_t expires)
{
	ExpireParm *exp_parm;
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	
	exp_parm = g_new (ExpireParm, 1);
	exp_parm->index = index;
	exp_parm->expires = expires;
	
	parms = seahorse_edit_parm_new (EXPIRE_START, edit_expire_action, edit_expire_transit, exp_parm);
	
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
edit_disable_transit (guint current_state, GpgmeStatusCode status,
		      const gchar *args, gpointer data, GpgmeError *err)
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

/**
 * seahorse_ops_key_set_disabled:
 * @sctx: Current context
 * @skey: Key to change
 * @disabled: Whether or not @skey should be disabled
 *
 * Disables or enables @skey depending on @disabled.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_key_set_disabled (SeahorseContext *sctx, SeahorseKey *skey, gboolean disabled)
{
	gchar *command, *op;
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	g_return_val_if_fail (disabled != gpgme_key_get_ulong_attr (
		skey->key, GPGME_ATTR_KEY_DISABLED, NULL, 0), FALSE);
	
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
edit_pass_transit (guint current_state, GpgmeStatusCode status,
		   const gchar *args, gpointer data, GpgmeError *err)
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

/**
 * seahorse_ops_key_change_pass:
 * @sctx: Current context
 * @skey: Key to change
 *
 * Allows user to change the passphrase for @skey.  Gpgme passphrase callback
 * must be set for @sctx.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 **/
gboolean
seahorse_ops_key_change_pass (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseEditParm *parms;
	GpgmePassphraseCb callback = NULL;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	
	/* Check if have passphrase callback */
	gpgme_get_passphrase_cb (sctx->ctx, &callback, NULL);
	g_return_val_if_fail (callback != NULL, FALSE);
	
	parms = seahorse_edit_parm_new (PASS_START, edit_pass_action, edit_pass_transit, NULL);
	
	return edit_key (sctx, skey, parms, _("Passphrase Change"), SKEY_CHANGE_PASS);
}

/**
 * seahorse_ops_key_check_email:
 * @email: Email entry to check
 *
 * Checks if @email appears to be a valid email address.
 *
 * Returns: %TRUE if @email appears valid, %FALSE otherwise
 **/
gboolean
seahorse_ops_key_check_email (const gchar *email)
{
	return g_pattern_match_simple ("?*@?*", email);
}

typedef enum {
	ADD_UID_START,
	ADD_UID_COMMAND,
	ADD_UID_NAME,
	ADD_UID_EMAIL,
	ADD_UID_COMMENT,
	ADD_UID_QUIT,
	ADD_UID_SAVE,
	ADD_UID_ERROR
} AddUidState;

typedef struct
{
	const gchar	*name;
	const gchar	*email;
	const gchar	*comment;
} UidParm;

static GpgmeError
add_uid_action (guint state, gpointer data, const gchar **result)
{
	UidParm *parm = (UidParm*)data;
	
	switch (state) {
		case ADD_UID_COMMAND:
			*result = "adduid";
			break;
		case ADD_UID_NAME:
			*result = parm->name;
			break;
		case ADD_UID_EMAIL:
			*result = parm->email;
			break;
		case ADD_UID_COMMENT:
			*result = parm->comment;
			break;
		case ADD_UID_QUIT:
			*result = QUIT;
			break;
		case ADD_UID_SAVE:
			*result = YES;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

static guint
add_uid_transit (guint current_state, GpgmeStatusCode status,
		 const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		case ADD_UID_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_UID_COMMAND;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_UID_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.name"))
				next_state = ADD_UID_NAME;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_UID_NAME:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.email"))
				next_state = ADD_UID_EMAIL;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_UID_EMAIL:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.comment"))
				next_state = ADD_UID_COMMENT;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_UID_COMMENT:
			next_state = ADD_UID_QUIT;
			break;
		case ADD_UID_QUIT:
			/* Quit, save */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = ADD_UID_SAVE;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_UID_ERROR:
			/* Go to quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_UID_QUIT;
			else
				next_state = ADD_UID_ERROR;
			break;
		default:
			next_state = ADD_UID_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

/**
 * seahorse_ops_key_add_uid:
 * @sctx: Current context
 * @skey: Key to edit
 * @name: New user ID name. Must be at least 5 characters long.
 * @email: New user ID email. Must appear to be a valid email address.
 * @comment: New user ID comment. Not required.
 *
 * Creates a new user ID for @skey.  The new ID will be the last user ID,
 * unless the primary user ID has not be specifically chosen, in which case
 * the new ID will become the primary ID.  @skey will the changed signal for
 * user IDs.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 **/
gboolean
seahorse_ops_key_add_uid (SeahorseContext *sctx, SeahorseKey *skey,
			  const gchar *name, const gchar *email, const gchar *comment)
{
	SeahorseEditParm *parms;
	UidParm *uid_parm;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	g_return_val_if_fail (name != NULL && strlen (name) >= 5, FALSE);
	g_return_val_if_fail (email != NULL && seahorse_ops_key_check_email (email), FALSE);
	
	uid_parm = g_new (UidParm, 1);
	uid_parm->name = name;
	uid_parm->email = email;
	uid_parm->comment = comment;
	
	parms = seahorse_edit_parm_new (ADD_UID_START, add_uid_action, add_uid_transit, uid_parm);
	
	return edit_key (sctx, skey, parms, _("Add User ID"), SKEY_CHANGE_UIDS);
}

typedef enum {
	ADD_KEY_START,
	ADD_KEY_COMMAND,
	ADD_KEY_TYPE,
	ADD_KEY_LENGTH,
	ADD_KEY_EXPIRES,
	ADD_KEY_PROGRESS,
	ADD_KEY_QUIT,
	ADD_KEY_SAVE,
	ADD_KEY_ERROR
} AddKeyState;

typedef struct
{
	SeahorseKeyType	type;
	guint		length;
	time_t		expires;
} SubkeyParm;

static GpgmeError
add_key_action (guint state, gpointer data, const gchar **result)
{
	SubkeyParm *parm = (SubkeyParm*)data;
	
	switch (state) {
		case ADD_KEY_COMMAND:
			*result = "addkey";
			break;
		case ADD_KEY_TYPE:
			*result = g_strdup_printf ("%d", parm->type);
			break;
		case ADD_KEY_LENGTH:
			*result = g_strdup_printf ("%d", parm->length);
			break;
		case ADD_KEY_EXPIRES:
			*result = (parm->expires) ?
				seahorse_util_get_date_string (parm->expires) : "0";
			break;
		case ADD_KEY_PROGRESS:
			break;
		case ADD_KEY_QUIT:
			*result = QUIT;
			break;
		case ADD_KEY_SAVE:
			*result = YES;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

static guint
add_key_transit (guint current_state, GpgmeStatusCode status,
		 const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;

	switch (current_state) {
		case ADD_KEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_KEY_COMMAND;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_KEY_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.algo"))
				next_state = ADD_KEY_TYPE;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_KEY_TYPE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.size"))
				next_state = ADD_KEY_LENGTH;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_KEY_LENGTH:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.valid"))
				next_state = ADD_KEY_EXPIRES;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_KEY_EXPIRES:
			next_state = ADD_KEY_PROGRESS;
			break;
		case ADD_KEY_PROGRESS:
			/* Loop in PROGRESS while creating key */
			if (status == GPGME_STATUS_PROGRESS ||
			status == GPGME_STATUS_KEY_CREATED)
				next_state = ADD_KEY_PROGRESS;
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_KEY_QUIT;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_KEY_QUIT:
			/* Quit, save */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = ADD_KEY_SAVE;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case ADD_KEY_ERROR:
			/* Go to quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_KEY_QUIT;
			else
				next_state = ADD_KEY_ERROR;
			break;
		default:
			next_state = ADD_KEY_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

gboolean
seahorse_ops_key_add_subkey (SeahorseContext *sctx, SeahorseKey *skey,
			     const SeahorseKeyType type, const guint length, const time_t expires)
{
	SeahorseEditParm *parms;
	SubkeyParm *key_parm;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	
	switch (type) {
		case DSA:
			g_return_val_if_fail (length >= DSA_MIN && length <= DSA_MAX, FALSE);
			break;
		case ELGAMAL:
			g_return_val_if_fail (length >= ELGAMAL_MIN && length <= LENGTH_MAX, FALSE);
			break;
		case RSA_SIGN: case RSA_ENCRYPT:
			g_return_val_if_fail (length >= RSA_MIN && length <= LENGTH_MAX, FALSE);
			break;
		default:
			g_return_val_if_reached (FALSE);
			break;
	}
	
	key_parm = g_new (SubkeyParm, 1);
	key_parm->type = type;
	key_parm->length = length;
	key_parm->expires = expires;
	
	parms = seahorse_edit_parm_new (ADD_KEY_START, add_key_action, add_key_transit, key_parm);
	
	return edit_key (sctx, skey, parms, _("Add Subkey"), SKEY_CHANGE_SUBKEYS);
}

typedef enum {
	DEL_KEY_START,
	DEL_KEY_SELECT,
	DEL_KEY_COMMAND,
	DEL_KEY_CONFIRM,
	DEL_KEY_QUIT,
	DEL_KEY_SAVE,
	DEL_KEY_ERROR
} DelKeyState;

static GpgmeError
del_key_action (guint state, gpointer data, const gchar **result)
{
	switch (state) {
		case DEL_KEY_SELECT:
			*result = g_strdup_printf ("key %d", (guint)data);
			break;
		case DEL_KEY_COMMAND:
			*result = "delkey";
			break;
		case DEL_KEY_CONFIRM:
			*result = YES;
			break;
		case DEL_KEY_QUIT:
			*result = QUIT;
			break;
		case DEL_KEY_SAVE:
			*result = YES;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

static guint
del_key_transit (guint current_state, GpgmeStatusCode status,
		 const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;

	switch (current_state) {
		case DEL_KEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_SELECT;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case DEL_KEY_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_COMMAND;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case DEL_KEY_COMMAND:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal
			(args, "keyedit.remove.subkey.okay"))
				next_state = DEL_KEY_CONFIRM;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case DEL_KEY_CONFIRM:
			next_state = DEL_KEY_QUIT;
			break;
		case DEL_KEY_QUIT:
			/* Quit, save */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = DEL_KEY_SAVE;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case DEL_KEY_ERROR:
			/* Go to quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_QUIT;
			else
				next_state = DEL_KEY_ERROR;
			break;
		default:
			next_state = DEL_KEY_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

gboolean
seahorse_ops_key_del_subkey (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_subkeys (skey), FALSE);
	
	parms = seahorse_edit_parm_new (DEL_KEY_START, del_key_action,
		del_key_transit, (gpointer)index);
	
	return edit_key (sctx, skey, parms, _("Delete Subkey"), SKEY_CHANGE_SUBKEYS);
}

typedef struct
{
	guint			index;
	SeahorseRevokeReason	reason;
	const gchar		*description;
} RevSubkeyParm;

typedef enum {
	REV_SUBKEY_START,
	REV_SUBKEY_SELECT,
	REV_SUBKEY_COMMAND,
	REV_SUBKEY_CONFIRM,
	REV_SUBKEY_REASON,
	REV_SUBKEY_DESCRIPTION,
	REV_SUBKEY_ENDDESC,
	REV_SUBKEY_QUIT,
	REV_SUBKEY_ERROR
} RevSubkeyStates;

static GpgmeError
rev_subkey_action (guint state, gpointer data, const gchar **result)
{
	RevSubkeyParm *parm = (RevSubkeyParm*)data;
	
	switch (state) {
		case REV_SUBKEY_SELECT:
			*result = g_strdup_printf ("key %d", parm->index);
			break;
		case REV_SUBKEY_COMMAND:
			*result = "revkey";
			break;
		case REV_SUBKEY_CONFIRM:
			*result = YES;
			break;
		case REV_SUBKEY_REASON:
			*result = g_strdup_printf ("%d", parm->reason);
			break;
		case REV_SUBKEY_DESCRIPTION:
			*result = g_strdup_printf ("%s\n", parm->description);
			break;
		case REV_SUBKEY_ENDDESC:
			*result = "\n";
			break;
		case REV_SUBKEY_QUIT:
			*result = QUIT;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

static guint
rev_subkey_transit (guint current_state, GpgmeStatusCode status,
		    const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	g_print ("last state %d\n", current_state);
	
	switch (current_state) {
		case REV_SUBKEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_SELECT;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_COMMAND;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_COMMAND:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "keyedit.revoke.subkey.okay"))
				next_state = REV_SUBKEY_CONFIRM;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_CONFIRM:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "ask_revocation_reason.code"))
				next_state = REV_SUBKEY_REASON;
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_QUIT;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_REASON:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "ask_revocation_reason.text"))
				next_state = REV_SUBKEY_DESCRIPTION;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_DESCRIPTION:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "ask_revocation_reason.text"))
				next_state = REV_SUBKEY_ENDDESC;
			else if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "ask_revocation_reason.okay"))
				next_state = REV_SUBKEY_CONFIRM;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_ENDDESC:
			next_state = REV_SUBKEY_CONFIRM;
			break;
		case REV_SUBKEY_QUIT:
			/* Quit, save */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = REV_SUBKEY_CONFIRM;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_ERROR:
			/* Go to quit */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_QUIT;
			else
				next_state = REV_SUBKEY_ERROR;
			break;
		default:
			next_state = REV_SUBKEY_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	g_print ("next state %d\n", next_state);
	
	return next_state;
}

gboolean
seahorse_ops_key_revoke_subkey (SeahorseContext *sctx, SeahorseKey *skey, const guint index,
				SeahorseRevokeReason reason, const gchar *description)
{
	RevSubkeyParm *rev_parm;
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (sctx != NULL && SEAHORSE_IS_CONTEXT (sctx), FALSE);
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_subkeys (skey), FALSE);
	
	rev_parm = g_new0 (RevSubkeyParm, 1);
	rev_parm->index = index;
	rev_parm->reason = reason;
	rev_parm->description = description;
	
	parms = seahorse_edit_parm_new (REV_SUBKEY_START, rev_subkey_action,
		rev_subkey_transit, rev_parm);
	
	return edit_key (sctx, skey, parms, _("Revoke Subkey"), SKEY_CHANGE_SUBKEYS);
}
