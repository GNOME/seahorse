
#include <gnome.h>

#include "seahorse-key-op.h"
#include "seahorse-util.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-libdialogs.h"

#define PROMPT "keyedit.prompt"
#define QUIT "quit"
#define SAVE "keyedit.save.okay"
#define YES "Y"

/**
 * seahorse_key_op_generate:
 * @sctx: #SeahorseContext
 * @name: User ID name, must be at least 5 characters long
 * @email: Optional user ID email
 * @comment: Optional user ID comment
 * @passphrase: Passphrase for key
 * @type: Key type. Supported types are #DSA_ELGAMAL, #DSA, and #RSA_SIGN
 * @length: Length of key, must be within the range of @type specified by #SeahorseKeyLength
 * @expires: Expiration date of key
 *
 * Tries to generate a new key based on given parameters.
 *
 * Returns: GpgmeError
 **/
GpgmeError
seahorse_key_op_generate (SeahorseContext *sctx, const gchar *name,
			  const gchar *email, const gchar *comment,
			  const gchar *passphrase, const SeahorseKeyType type,
			  const guint length, const time_t expires)
{
	gchar *common, *key_type, *start, *parms, *expires_date;
        GpgmeError err;
	
	g_return_val_if_fail (strlen (name) >= 5, GPGME_Invalid_Value);
	
	/* Check lengths for each type */
	switch (type) {
		case DSA_ELGAMAL:
			g_return_val_if_fail (length >= ELGAMAL_MIN && length <= LENGTH_MAX, GPGME_Invalid_Value);
			break;
		case DSA:
			g_return_val_if_fail (length >= DSA_MIN && length <= DSA_MAX, GPGME_Invalid_Value);
			break;
		case RSA_SIGN:
			g_return_val_if_fail (length >= RSA_MIN && length <= LENGTH_MAX, GPGME_Invalid_Value);
			break;
		default:
			g_return_val_if_reached (GPGME_Invalid_Value);
			break;
	}
	
	if (expires != 0)
		expires_date = seahorse_util_get_date_string (expires);
	else
		expires_date = "0";
	
	/* Common xml */
	common = g_strdup_printf ("Name-Real: %s\nExpire-Date: %s\nPassphrase: %s\n"
		"</GnupgKeyParms>", name, expires_date, passphrase);
	if (email != NULL && strlen (email) > 0)
		common = g_strdup_printf ("Name-Email: %s\n%s", email, common);
	if (comment != NULL && strlen (comment) > 0)
		common = g_strdup_printf ("Name-Comment: %s\n%s", comment, common);
	
	if (type == RSA_SIGN)
		key_type = "Key-Type: RSA";
	else
		key_type = "Key-Type: DSA";
	
	start = g_strdup_printf ("<GnupgKeyParms format=\"internal\">\n%s\nKey-Length: ", key_type);
	
	/* Subkey xml */
	if (type == DSA_ELGAMAL)
		parms = g_strdup_printf ("%s%d\nSubkey-Type: ELG-E\nSubkey-Length: %d\n%s",
					 start, DSA_MAX, length, common);
	else
		parms = g_strdup_printf ("%s%d\n%s", start, length, common);
	
	err = gpgme_op_genkey (sctx->ctx, parms, NULL, NULL);
	
	/* Free xmls */
	g_free (parms);
	g_free (start);
	g_free (key_type);
	g_free (common);
	g_free (expires_date);
	
	return err;
}

/* helper function for deleting @skey */
static GpgmeError
op_delete (SeahorseContext *sctx, SeahorseKey *skey, gboolean secret)
{
	GpgmeError err;
	
	err = gpgme_op_delete (sctx->ctx, skey->key, secret);
	if (err == GPGME_No_Error)
		seahorse_key_destroy (skey);
	
	return err;
}

/**
 * seahorse_key_op_delete:
 * @sctx: #SeahorseContext
 * @skey: #SeahorseKey to delete
 *
 * Tries to delete the key @skey.
 *
 * Returns: GpgmeError
 **/
GpgmeError
seahorse_key_op_delete (SeahorseContext *sctx, SeahorseKey *skey)
{
	return op_delete (sctx, skey, FALSE);
}

/**
 * seahorse_key_pair_op_delete:
 * @sctx: #SeahorseContext
 * @skpair: #SeahorseKeyPair to delete
 *
 * Tries to delete the key pair @skpair.
 *
 * Returns: GpgmeError
 **/
GpgmeError
seahorse_key_pair_op_delete (SeahorseContext *sctx, SeahorseKeyPair *skpair)
{
	return op_delete (sctx, SEAHORSE_KEY (skpair), TRUE);
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
seahorse_key_op_edit (gpointer data, GpgmeStatusCode status,
		      const gchar *args, const gchar **result)
{
	SeahorseEditParm *parms = (SeahorseEditParm*)data;
	
	/* Ignore these status lines, as they don't require any response */
	if (status == GPGME_STATUS_EOF || status == GPGME_STATUS_GOT_IT ||
	    status == GPGME_STATUS_NEED_PASSPHRASE || status == GPGME_STATUS_GOOD_PASSPHRASE ||
	    status == GPGME_STATUS_BAD_PASSPHRASE || status == GPGME_STATUS_USERID_HINT ||
	    status == GPGME_STATUS_SIGEXPIRED || status == GPGME_STATUS_KEYEXPIRED ||
	    status == GPGME_STATUS_PROGRESS || status == GPGME_STATUS_KEY_CREATED ||
	    status == GPGME_STATUS_ALREADY_SIGNED)
		return parms->err;

	/* Choose the next state based on the current one and the input */
	parms->state = parms->transit (parms->state, status, args, parms->data, &parms->err);
	
	/* Choose the action based on the state */
	if (parms->err == GPGME_No_Error)
		parms->err = parms->action (parms->state, parms->data, result);
	
	return parms->err;
}

/* Common edit operation */
static GpgmeError
edit_key (SeahorseContext *sctx, SeahorseKey *skey, SeahorseEditParm *parms, SeahorseKeyChange change)
{
	GpgmeData out;
	GpgmeError err;
	
	err = gpgme_data_new (&out);
	g_return_val_if_fail (err == GPGME_No_Error, err);
	/* do edit callback, release data */
	err = gpgme_op_edit (sctx->ctx, skey->key, seahorse_key_op_edit, parms, out);
	gpgme_data_release (out);
	g_return_val_if_fail (err == GPGME_No_Error, err);
	/* signal key */
	seahorse_key_changed (skey, change);
	return err;
}

typedef struct
{
	guint			index;
	gchar			*command;
	gboolean		expire;
	SeahorseSignCheck	check;
} SignParm;

typedef enum
{
	SIGN_START,
	SIGN_UID,
	SIGN_COMMAND,
	SIGN_EXPIRE,
	SIGN_CONFIRM,
	SIGN_CHECK,
	SIGN_QUIT,
	SIGN_ERROR
} SignState;

/* action helper for signing a key */
static GpgmeError
sign_action (guint state, gpointer data, const gchar **result)
{
	SignParm *parm = (SignParm*)data;
	
	switch (state) {
		/* select uid */
		case SIGN_UID:
			*result = g_strdup_printf ("uid %d", parm->index);
			break;
		case SIGN_COMMAND:
			*result = parm->command;
			break;
		/* if expires */
		case SIGN_EXPIRE:
			*result = (parm->expire) ? YES : "N";
			break;
		case SIGN_CONFIRM:
			*result = YES;
			break;
		case SIGN_CHECK:
			*result = g_strdup_printf ("%d", parm->check);
			break;
		case SIGN_QUIT:
			*result = QUIT;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

/* transition helper for signing a key */
static guint
sign_transit (guint current_state, GpgmeStatusCode status,
	      const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start state, need to select uid */
		case SIGN_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = SIGN_UID;
			else {
				next_state = SIGN_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* selected uid, go to command */
		case SIGN_UID:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = SIGN_COMMAND;
			else {
				next_state = SIGN_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case SIGN_COMMAND:
			/* if doing all uids, confirm */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "keyedit.sign_all.okay"))
				next_state = SIGN_CONFIRM;
			/* need to do expires */
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "sign_uid.expire"))
				next_state = SIGN_EXPIRE;
			/*  need to do check */
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "sign_uid.class"))
				next_state = SIGN_CHECK;
			else {
				next_state = SIGN_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did expire, go to check */
		case SIGN_EXPIRE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "sign_uid.class"))
				next_state = SIGN_CHECK;
			else {
				next_state = SIGN_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case SIGN_CONFIRM:
			/* need to do check */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "sign_uid.class"))
				next_state = SIGN_CHECK;
			/* need to do expire */
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "sign_uid.expire"))
				next_state = SIGN_EXPIRE;
			/* quit */
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = SIGN_QUIT;
			else {
				next_state = SIGN_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did check, go to confirm */
		case SIGN_CHECK:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "sign_uid.okay"))
				next_state = SIGN_CONFIRM;
			else {
				next_state = SIGN_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* quit, go to confirm to save */
		case SIGN_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = SIGN_CONFIRM;
			else {
				next_state = SIGN_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, go to quit */
		case SIGN_ERROR:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = SIGN_QUIT;
			else
				next_state = SIGN_ERROR;
			break;
		default:
			next_state = SIGN_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_op_sign:
 * @sctx: #SeahorseContext
 * @skey: #SeahorseKey to sign
 * @index: User ID to sign, 0 is all user IDs
 * @check: #SeahorseSignCheck
 * @options: #SeahorseSignOptions
 *
 * Tries to sign user ID @index of @skey with the default key and the given options.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_op_sign (SeahorseContext *sctx, SeahorseKey *skey, const guint index,
		      SeahorseSignCheck check, SeahorseSignOptions options)
{
	SignParm *sign_parm;
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPGME_Invalid_Key);
	g_return_val_if_fail (index <= seahorse_key_get_num_uids (skey), GPGME_Invalid_Value);
	
	sign_parm = g_new (SignParm, 1);
	sign_parm->index = index;
	sign_parm->expire = ((options & SIGN_EXPIRES) != 0);
	sign_parm->check = check;
	sign_parm->command = "sign";
	
	/* if sign is local */
	if ((options & SIGN_LOCAL) != 0)
		sign_parm->command = g_strdup_printf ("l%s", sign_parm->command);
	/* if sign is non-revocable */
	if ((options & SIGN_NO_REVOKE) != 0)
		sign_parm->command = g_strdup_printf ("nr%s", sign_parm->command);
	
	parms = seahorse_edit_parm_new (SIGN_START, sign_action, sign_transit, sign_parm);
	
	return edit_key (sctx, skey, parms, SKEY_CHANGE_SIGNERS);
}

typedef enum {
	PASS_START,
	PASS_COMMAND,
	PASS_QUIT,
	PASS_SAVE,
	PASS_ERROR
} PassState;

/* action helper for changing passphrase */
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
	
	return GPGME_No_Error;
}

/* transition helper for changing passphrase */
static guint
edit_pass_transit (guint current_state, GpgmeStatusCode status,
		   const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start state, go to command */
		case PASS_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = PASS_COMMAND;
			else {
				next_state = PASS_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, go to quit */
		case PASS_COMMAND:
			next_state = PASS_QUIT;
			break;
		/* quit, go to save */
		case PASS_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = PASS_SAVE;
			else {
				next_state = PASS_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, go to quit */
		case PASS_ERROR:
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

/* helper func for changing the passphrase */
static const gchar*
edit_pass_get (SeahorseContext *sctx, const gchar *desc, gpointer *data)
{
	static gint count = 0;
	
	/* if not showing callback */
	if (!desc)
		return NULL;
	/* if doing first time entry */
	if (desc[0] == 'E')
		count++;
	/* if need original passphrase */
	if (count == 1)
		return seahorse_passphrase_get (sctx, desc, data);
	/* otherwise need new passphrase */
	else {
		count = 0;
		return seahorse_change_passphrase_get (sctx, desc, data);
	}
}

/**
 * seahorse_key_pair_op_change_pass:
 * @sctx: #SeahorseContext
 * @skpair: #SeahorseKeyPair whose passphrase to change
 *
 * Tries to change the passphrase of @skpair. Passphrase entry is done with
 * seahorse_passphrase_get() and seahorse_change_passphrase_get().
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_pair_op_change_pass (SeahorseContext *sctx, SeahorseKeyPair *skpair)
{
	SeahorseEditParm *parms;
	GpgmeError err;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPGME_Invalid_Key);
	
	parms = seahorse_edit_parm_new (PASS_START, edit_pass_action, edit_pass_transit, NULL);
	
	/* change passphrase callback to helper func, edit key, change callback to regular func */
	gpgme_set_passphrase_cb (sctx->ctx, (GpgmePassphraseCb)edit_pass_get, sctx);
	err = edit_key (sctx, SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_PASS);
	gpgme_set_passphrase_cb (sctx->ctx, (GpgmePassphraseCb)seahorse_passphrase_get, sctx);
	
	return err;
}

typedef enum
{
	TRUST_START,
	TRUST_COMMAND,
	TRUST_VALUE,
	TRUST_CONFIRM,
	TRUST_QUIT,
	TRUST_ERROR
} TrustState;

/* action helper for setting trust of a key */
static GpgmeError
edit_trust_action (guint state, gpointer data, const gchar **result)
{
	SeahorseValidity trust = (SeahorseValidity)data;
	
	switch (state) {
		/* enter command */
		case TRUST_COMMAND:
			*result = "trust";
			break;
		/* enter numeric trust value */
		case TRUST_VALUE:
			*result = g_strdup_printf ("%d", trust);
			break;
		/* confirm ultimate or if save */
		case TRUST_CONFIRM:
			*result = YES;
			break;
		/* quit */
		case TRUST_QUIT:
			*result = QUIT;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

/* transition helper for setting trust of a key */
static guint
edit_trust_transit (guint current_state, GpgmeStatusCode status,
		    const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start state */
		case TRUST_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_COMMAND;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, next is value */
		case TRUST_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "edit_ownertrust.value"))
				next_state = TRUST_VALUE;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did value, go to quit or confirm ultimate */
		case TRUST_VALUE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_QUIT;
			else if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "edit_ownertrust.set_ultimate.okay"))
				next_state = TRUST_CONFIRM;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did confirm ultimate, go to quit */
		case TRUST_CONFIRM:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_QUIT;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did quit, go to confirm to finish op */
		case TRUST_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = TRUST_CONFIRM;
			else {
				next_state = TRUST_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, go to quit */
		case TRUST_ERROR:
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
 * seahorse_key_op_set_trust:
 * @sctx: #SeahorseContext
 * @skey: #SeahorseKey whose trust will be changed
 * @trust: New trust value that must be at least SEAHORSE_VALIDITY_UNKNOWN.
 * If @skey is a #SeahorseKeyPair, then @trust cannot be SEAHORSE_VALIDITY_UNKNOWN.
 * If @skey is not a #SeahorseKeyPair, then @trust cannot be SEAHORSE_VALIDITY_ULTIMATE.
 *
 * Tries to change the owner trust of @skey to @trust.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_op_set_trust (SeahorseContext *sctx, SeahorseKey *skey, SeahorseValidity trust)
{
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPGME_Invalid_Key);
	g_return_val_if_fail (trust < GPGME_VALIDITY_UNKNOWN, GPGME_Invalid_Value);
	g_return_val_if_fail (seahorse_key_get_trust (skey) != trust, GPGME_Invalid_Value);
	
	if (SEAHORSE_IS_KEY_PAIR (skey))
		g_return_val_if_fail (trust != SEAHORSE_VALIDITY_UNKNOWN, GPGME_Invalid_Value);
	else
		g_return_val_if_fail (trust != SEAHORSE_VALIDITY_ULTIMATE, GPGME_Invalid_Value);
	
	parms = seahorse_edit_parm_new (TRUST_START, edit_trust_action,
		edit_trust_transit, (gpointer)trust);
	
	return edit_key (sctx, skey, parms, SKEY_CHANGE_TRUST);
}

typedef enum {
	DISABLE_START,
	DISABLE_COMMAND,
	DISABLE_QUIT,
	DISABLE_ERROR
} DisableState;

/* action helper for disable/enable a key */
static GpgmeError
edit_disable_action (guint state, gpointer data, const gchar **result)
{
	const gchar *command = data;
	
	switch (state) {
		case DISABLE_COMMAND:
			*result = command;
			break;
		case DISABLE_QUIT:
			*result = QUIT;
			break;
		default:
			break;
	}
	
	return GPGME_No_Error;
}

/* transition helper for disable/enable a key */
static guint
edit_disable_transit (guint current_state, GpgmeStatusCode status,
		      const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, do command */
		case DISABLE_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_COMMAND;
			else {
				next_state = DISABLE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, quit */
		case DISABLE_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_QUIT;
			else {
				next_state = DISABLE_ERROR;
				*err = GPGME_General_Error;
			}
		/* error, quit */
		case DISABLE_ERROR:
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
 * seahorse_key_op_set_disabled:
 * @sctx: #SeahorseContext
 * @skey: #SeahorseKey to change
 * @disabled: New disabled state
 *
 * Tries to change disabled state of @skey to @disabled.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_op_set_disabled (SeahorseContext *sctx, SeahorseKey *skey, gboolean disabled)
{
	gchar *command;
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPGME_Invalid_Key);
	/* Make sure changing disabled */
	g_return_val_if_fail (disabled != gpgme_key_get_ulong_attr (
		skey->key, GPGME_ATTR_KEY_DISABLED, NULL, 0), GPGME_Invalid_Value);
	/* Get command and op */
	if (disabled)
		command = "disable";
	else
		command = "enable";
	
	parms = seahorse_edit_parm_new (DISABLE_START, edit_disable_action, edit_disable_transit, command);
	g_free (command);
	
	return edit_key (sctx, skey, parms, SKEY_CHANGE_DISABLED);
}

typedef struct
{
	guint	index;
	time_t	expires;
} ExpireParm;

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

/* action helper for changing expiration date of a key */
static GpgmeError
edit_expire_action (guint state, gpointer data, const gchar **result)
{
	ExpireParm *parm = (ExpireParm*)data;
  
	switch (state) {
		/* selected key */
		case EXPIRE_SELECT:
			*result = g_strdup_printf ("key %d", parm->index);
			break;
		case EXPIRE_COMMAND:
			*result = "expire";
			break;
		/* set date */
		case EXPIRE_DATE:
			*result = (parm->expires) ?
				seahorse_util_get_date_string (parm->expires) : "0";
			break;
		case EXPIRE_QUIT:
			*result = QUIT;
			break;
		case EXPIRE_SAVE:
			*result = YES;
			break;
		case EXPIRE_ERROR:
			break;
		default:
			return GPGME_General_Error;
	}
	return GPGME_No_Error;
}

/* transition helper for changing expiration date of a key */
static guint
edit_expire_transit (guint current_state, GpgmeStatusCode status,
		     const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
 
	switch (current_state) {
		/* start state, selected key */
		case EXPIRE_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_SELECT;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* selected key, do command */
		case EXPIRE_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_COMMAND;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, set expires */
		case EXPIRE_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.valid"))
				next_state = EXPIRE_DATE;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* set expires, quit */
		case EXPIRE_DATE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_QUIT;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* quit, save */
		case EXPIRE_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = EXPIRE_SAVE;
			else {
				next_state = EXPIRE_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, quit */
		case EXPIRE_ERROR:
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
 * seahorse_key_pair_op_set_expires:
 * @sctx: #SeahorseContext
 * @skpair: #SeahorseKeyPair whose expiration date to change
 * @index: Index of key to change, 0 being the primary key
 * @expires: New expiration date, 0 being never
 *
 * Tries to change the expiration date of the key at @index of @skpair to @expires.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_pair_op_set_expires (SeahorseContext *sctx, SeahorseKeyPair *skpair,
				  const guint index, const time_t expires)
{
	ExpireParm *exp_parm;
	SeahorseEditParm *parms;
	SeahorseKey *skey;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPGME_Invalid_Key);
	skey = SEAHORSE_KEY (skpair);
	/* Make sure changing expires */
	g_return_val_if_fail (expires != gpgme_key_get_ulong_attr (skey->key,
		GPGME_ATTR_EXPIRE, NULL, index), FALSE);
	
	exp_parm = g_new (ExpireParm, 1);
	exp_parm->index = index;
	exp_parm->expires = expires;
	
	parms = seahorse_edit_parm_new (EXPIRE_START, edit_expire_action, edit_expire_transit, exp_parm);
	
	return edit_key (sctx, skey, parms, SKEY_CHANGE_EXPIRES);
}

typedef enum {
	ADD_REVOKER_START,
	ADD_REVOKER_COMMAND,
	ADD_REVOKER_SELECT,
	ADD_REVOKER_CONFIRM,
	ADD_REVOKER_QUIT,
	ADD_REVOKER_ERROR
} AddRevokerState;

/* action helper for adding a revoker */
static GpgmeError
add_revoker_action (guint state, gpointer data, const gchar **result)
{
	gchar *keyid = (gchar*)data;
	
	switch (state) {
		case ADD_REVOKER_COMMAND:
			*result = "addrevoker";
			break;
		/* select revoker */
		case ADD_REVOKER_SELECT:
			*result = keyid;
			break;
		case ADD_REVOKER_CONFIRM:
			*result = YES;
			break;
		case ADD_REVOKER_QUIT:
			*result = QUIT;
			break;
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

/* transition helper for adding a revoker */
static guint
add_revoker_transit (guint current_state, GpgmeStatusCode status,
		     const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, do command */
		case ADD_REVOKER_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_REVOKER_COMMAND;
			else {
				next_state = ADD_REVOKER_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, select revoker */
		case ADD_REVOKER_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keyedit.add_revoker"))
				next_state = ADD_REVOKER_SELECT;
			else {
				next_state = ADD_REVOKER_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* selected revoker, confirm */
		case ADD_REVOKER_SELECT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "keyedit.add_revoker.okay"))
				next_state = ADD_REVOKER_CONFIRM;
			else {
				next_state = ADD_REVOKER_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* confirmed, quit */
		case ADD_REVOKER_CONFIRM:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_REVOKER_QUIT;
			else {
				next_state = ADD_REVOKER_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* quit, confirm(=save) */
		case ADD_REVOKER_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = ADD_REVOKER_CONFIRM;
			else {
				next_state = ADD_REVOKER_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, quit */
		case ADD_REVOKER_ERROR:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_REVOKER_QUIT;
			else
				next_state = ADD_REVOKER_ERROR;
			break;
		default:
			next_state = ADD_REVOKER_ERROR;
			*err = GPGME_General_Error;
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_pair_op_add_revoker:
 * @sctx: #SeahorseContext
 * @skpair: #SeahorseKeyPair to add a revoker to
 *
 * Tries to add the default key as a revoker for @skpair.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_pair_op_add_revoker (SeahorseContext *sctx, SeahorseKeyPair *skpair)
{
	SeahorseEditParm *parms;
	SeahorseKey *revoker;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPGME_Invalid_Key);
	
	revoker = SEAHORSE_KEY (seahorse_context_get_default_key (sctx));
	g_return_val_if_fail (revoker != NULL, GPGME_Invalid_Key);
	
	parms = seahorse_edit_parm_new (ADD_REVOKER_START, add_revoker_action,
		add_revoker_transit, (gpointer)seahorse_key_get_id (revoker->key));
	
	return edit_key (sctx, SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_REVOKERS);
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

/* action helper for adding a new user ID */
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

/* transition helper for adding a new user ID */
static guint
add_uid_transit (guint current_state, GpgmeStatusCode status,
		 const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, do command */
		case ADD_UID_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_UID_COMMAND;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, do name */
		case ADD_UID_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.name"))
				next_state = ADD_UID_NAME;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did name, do email */
		case ADD_UID_NAME:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.email"))
				next_state = ADD_UID_EMAIL;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did email, do comment */
		case ADD_UID_EMAIL:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.comment"))
				next_state = ADD_UID_COMMENT;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did comment, quit */
		case ADD_UID_COMMENT:
			next_state = ADD_UID_QUIT;
			break;
		/* quit, save */
		case ADD_UID_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = ADD_UID_SAVE;
			else {
				next_state = ADD_UID_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, quit */
		case ADD_UID_ERROR:
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
 * seahorse_key_pair_op_add_uid:
 * @sctx: #SeahorseContext
 * @skpair: #SeahorseKeyPair to add a user ID to
 * @name: New user ID name. Must be at least 5 characters long
 * @email: Optional email address
 * @comment: Optional comment
 *
 * Tries to add a new user ID to @skpair with the given @name, @email, and @comment.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_pair_op_add_uid (SeahorseContext *sctx, SeahorseKeyPair *skpair,
			      const gchar *name, const gchar *email, const gchar *comment)
{
	SeahorseEditParm *parms;
	UidParm *uid_parm;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPGME_Invalid_Key);
	g_return_val_if_fail (name != NULL && strlen (name) >= 5, GPGME_Invalid_Value);
	
	uid_parm = g_new (UidParm, 1);
	uid_parm->name = name;
	uid_parm->email = email;
	uid_parm->comment = comment;
	
	parms = seahorse_edit_parm_new (ADD_UID_START, add_uid_action, add_uid_transit, uid_parm);
	
	return edit_key (sctx, SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_UIDS);
}

typedef enum {
	ADD_KEY_START,
	ADD_KEY_COMMAND,
	ADD_KEY_TYPE,
	ADD_KEY_LENGTH,
	ADD_KEY_EXPIRES,
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

/* action helper for adding a subkey */
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
		/* Get exact date or 0 */
		case ADD_KEY_EXPIRES:
			*result = (parm->expires) ?
				seahorse_util_get_date_string (parm->expires) : "0";
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

/* transition helper for adding a subkey */
static guint
add_key_transit (guint current_state, GpgmeStatusCode status,
		 const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;

	switch (current_state) {
		/* start, do command */
		case ADD_KEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_KEY_COMMAND;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, do type */
		case ADD_KEY_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.algo"))
				next_state = ADD_KEY_TYPE;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did type, do length */
		case ADD_KEY_TYPE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.size"))
				next_state = ADD_KEY_LENGTH;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did length, do expires */
		case ADD_KEY_LENGTH:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.valid"))
				next_state = ADD_KEY_EXPIRES;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did expires, quit */
		case ADD_KEY_EXPIRES:
			next_state = ADD_KEY_QUIT;
			break;
		/* quit, save */
		case ADD_KEY_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = ADD_KEY_SAVE;
			else {
				next_state = ADD_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, quit */
		case ADD_KEY_ERROR:
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

/**
 * seahorse_key_pair_op_add_subkey:
 * @sctx: #SeahorseContext
 * @skpair: #SeahorseKeyPair to add a subkey to
 * @type: #SeahorseKeyType of new subkey, must be DSA, ELGAMAL, or an RSA type
 * @length: Length of new subkey, must be within #SeahorseKeyLength ranges for @type
 * @expires: Expiration date of new subkey, 0 being never
 *
 * Tries to add a new subkey to @skpair given @type, @length, and @expires.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_pair_op_add_subkey (SeahorseContext *sctx, SeahorseKeyPair *skpair,
				 const SeahorseKeyType type, const guint length,
				 const time_t expires)
{
	SeahorseEditParm *parms;
	SubkeyParm *key_parm;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPGME_Invalid_Key);
	
	/* Check length range & type */
	switch (type) {
		case DSA:
			g_return_val_if_fail (length >= DSA_MIN && length <= DSA_MAX, GPGME_Invalid_Value);
			break;
		case ELGAMAL:
			g_return_val_if_fail (length >= ELGAMAL_MIN && length <= LENGTH_MAX, GPGME_Invalid_Value);
			break;
		case RSA_SIGN: case RSA_ENCRYPT:
			g_return_val_if_fail (length >= RSA_MIN && length <= LENGTH_MAX, GPGME_Invalid_Value);
			break;
		default:
			g_return_val_if_reached (GPGME_Invalid_Value);
			break;
	}
	
	key_parm = g_new (SubkeyParm, 1);
	key_parm->type = type;
	key_parm->length = length;
	key_parm->expires = expires;
	
	parms = seahorse_edit_parm_new (ADD_KEY_START, add_key_action, add_key_transit, key_parm);
	
	return edit_key (sctx, SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_SUBKEYS);
}

typedef enum {
	DEL_KEY_START,
	DEL_KEY_SELECT,
	DEL_KEY_COMMAND,
	DEL_KEY_CONFIRM,
	DEL_KEY_QUIT,
	DEL_KEY_ERROR
} DelKeyState;

/* action helper for deleting a subkey */
static GpgmeError
del_key_action (guint state, gpointer data, const gchar **result)
{
	switch (state) {
		/* select key */
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
		default:
			return GPGME_General_Error;
	}
	
	return GPGME_No_Error;
}

/* transition helper for deleting a subkey */
static guint
del_key_transit (guint current_state, GpgmeStatusCode status,
		 const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;

	switch (current_state) {
		/* start, select key */
		case DEL_KEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_SELECT;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* selected key, do command */
		case DEL_KEY_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_COMMAND;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case DEL_KEY_COMMAND:
			/* did command, confirm */
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal
			(args, "keyedit.remove.subkey.okay"))
				next_state = DEL_KEY_CONFIRM;
			/* did command, quit */
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_QUIT;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* confirmed, quit */
		case DEL_KEY_CONFIRM:
			next_state = DEL_KEY_QUIT;
			break;
		/* quit, confirm(=save) */
		case DEL_KEY_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = DEL_KEY_CONFIRM;
			else {
				next_state = DEL_KEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, quit */
		case DEL_KEY_ERROR:
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

/**
 * seahorse_key_op_del_subkey:
 * @sctx: #SeahorseContext
 * @skey: #SeahorseKey whose subkey to delete
 * @index: Index of subkey to delete, must be at least 1
 *
 * Tries to delete subkey @index from @skey.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_op_del_subkey (SeahorseContext *sctx, SeahorseKey *skey, const guint index)
{
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPGME_Invalid_Key);
	g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_subkeys (skey), GPGME_Invalid_Value);
	
	parms = seahorse_edit_parm_new (DEL_KEY_START, del_key_action,
		del_key_transit, (gpointer)index);
	
	return edit_key (sctx, skey, parms, SKEY_CHANGE_SUBKEYS);
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
} RevSubkeyState;

/* action helper for revoking a subkey */
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
		/* Need empty line */
		case REV_SUBKEY_ENDDESC:
			*result = "\n";
			break;
		case REV_SUBKEY_QUIT:
			*result = QUIT;
			break;
		default:
			g_return_val_if_reached (GPGME_General_Error);
	}
	
	return GPGME_No_Error;
}

/* transition helper for revoking a subkey */
static guint
rev_subkey_transit (guint current_state, GpgmeStatusCode status,
		    const gchar *args, gpointer data, GpgmeError *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, select key */
		case REV_SUBKEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_SELECT;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* selected key, do command */
		case REV_SUBKEY_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_COMMAND;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did command, confirm */
		case REV_SUBKEY_COMMAND:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "keyedit.revoke.subkey.okay"))
				next_state = REV_SUBKEY_CONFIRM;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_CONFIRM:
			/* did confirm, do reason */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "ask_revocation_reason.code"))
				next_state = REV_SUBKEY_REASON;
			/* did confirm, quit */
			else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_QUIT;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* did reason, do description */
		case REV_SUBKEY_REASON:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "ask_revocation_reason.text"))
				next_state = REV_SUBKEY_DESCRIPTION;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		case REV_SUBKEY_DESCRIPTION:
			/* did description, end it */
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "ask_revocation_reason.text"))
				next_state = REV_SUBKEY_ENDDESC;
			/* did description, confirm */
			else if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "ask_revocation_reason.okay"))
				next_state = REV_SUBKEY_CONFIRM;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* ended description, confirm */
		case REV_SUBKEY_ENDDESC:
			next_state = REV_SUBKEY_CONFIRM;
			break;
		/* quit, confirm(=save) */
		case REV_SUBKEY_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = REV_SUBKEY_CONFIRM;
			else {
				next_state = REV_SUBKEY_ERROR;
				*err = GPGME_General_Error;
			}
			break;
		/* error, quit */
		case REV_SUBKEY_ERROR:
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
	
	return next_state;
}

/**
 * seahorse_key_op_revoke_subkey:
 * @sctx: #SeahorseContext
 * @skey: #SeahorseKey whose subkey to revoke
 * @index: Index of subkey to revoke, must be at least 1
 * @reason: #SeahorseRevokeReason for revoking the key
 * @description: Optional description of revocation
 *
 * Tries to revoke subkey @index of @skey given @reason and @description.
 *
 * Returns: Error value
 **/
GpgmeError
seahorse_key_op_revoke_subkey (SeahorseContext *sctx, SeahorseKey *skey, const guint index,
			       SeahorseRevokeReason reason, const gchar *description)
{
	RevSubkeyParm *rev_parm;
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), GPGME_Invalid_Engine);
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPGME_Invalid_Key);
	/* Check index range */
	g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_subkeys (skey), GPGME_Invalid_Value);
	/* Make sure not revoked */
	g_return_val_if_fail (!gpgme_key_get_ulong_attr (skey->key,
		GPGME_ATTR_KEY_REVOKED, NULL, index), GPGME_Invalid_Value);
	
	rev_parm = g_new0 (RevSubkeyParm, 1);
	rev_parm->index = index;
	rev_parm->reason = reason;
	rev_parm->description = description;
	
	parms = seahorse_edit_parm_new (REV_SUBKEY_START, rev_subkey_action,
		rev_subkey_transit, rev_parm);
	
	return edit_key (sctx, skey, parms, SKEY_CHANGE_SUBKEYS);
}
