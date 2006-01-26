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

#include "seahorse-gpgmex.h"
#include "seahorse-key-op.h"
#include "seahorse-util.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-libdialogs.h"

#define PROMPT "keyedit.prompt"
#define QUIT "quit"
#define SAVE "keyedit.save.okay"
#define YES "Y"

#define PRINT(args)  if(!seahorse_util_print_fd args) return GPG_E (GPG_ERR_GENERAL)
#define PRINTF(args) if(!seahorse_util_printf_fd args) return GPG_E (GPG_ERR_GENERAL)

/**
 * seahorse_key_op_generate:
 * @sksrc: #SeahorseKeySource
 * @name: User ID name, must be at least 5 characters long
 * @email: Optional user ID email
 * @comment: Optional user ID comment
 * @passphrase: Passphrase for key
 * @type: Key type. Supported types are #DSA_ELGAMAL, #DSA, and #RSA_SIGN
 * @length: Length of key, must be within the range of @type specified by #SeahorseKeyLength
 * @expires: Expiration date of key
 *
 * Tries to generate a new key based on given parameters.
 * The generation operation is done with a new GPGME context
 * If the generation is successful, seahorse_key_source_refresh() will be
 * called with @sksrc.
 *
 * Returns: gpgme_error_t
 **/
gpgme_error_t
seahorse_key_op_generate (SeahorseKeySource *sksrc, const gchar *name,
			  const gchar *email, const gchar *comment,
			  const gchar *passphrase, const SeahorseKeyType type,
			  const guint length, const time_t expires)
{
	gchar *common, *key_type, *start, *parms, *expires_date;
    gpgme_error_t err;
	gpgme_ctx_t new_ctx;
	
	g_return_val_if_fail (strlen (name) >= 5, GPG_E (GPG_ERR_INV_VALUE));
	
	/* Check lengths for each type */
	switch (type) {
		case DSA_ELGAMAL:
			g_return_val_if_fail (length >= ELGAMAL_MIN && length <= LENGTH_MAX, GPG_E (GPG_ERR_INV_VALUE));
			break;
		case DSA:
			g_return_val_if_fail (length >= DSA_MIN && length <= DSA_MAX, GPG_E (GPG_ERR_INV_VALUE));
			break;
		case RSA_SIGN:
			g_return_val_if_fail (length >= RSA_MIN && length <= LENGTH_MAX, GPG_E (GPG_ERR_INV_VALUE));
			break;
		default:
			g_return_val_if_reached (GPG_E (GPG_ERR_INV_VALUE));
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

    new_ctx = seahorse_key_source_new_context (sksrc);	
    g_return_val_if_fail (new_ctx != NULL, GPG_E (GPG_ERR_GENERAL));
    
	err = gpgme_op_genkey (new_ctx, parms, NULL, NULL);
    gpgme_release (new_ctx);
	
	if (GPG_IS_OK (err))
        seahorse_key_source_refresh_async (sksrc, SEAHORSE_KEY_SOURCE_NEW);
	
	/* Free xmls */
	g_free (parms);
	g_free (start);
	g_free (common);
	
	return err;
}

/* helper function for deleting @skey */
static gpgme_error_t
op_delete (SeahorseKey *skey, gboolean secret)
{
    SeahorseKeySource *sksrc;
	gpgme_error_t err;
    
    sksrc = seahorse_key_get_source (skey);
    g_return_val_if_fail (sksrc != NULL, GPG_E (GPG_ERR_INV_KEYRING));
	
	err = gpgme_op_delete (sksrc->ctx, skey->key, secret);
	if (GPG_IS_OK (err))
        seahorse_key_destroy (skey);
	
	return err;
}

/**
 * seahorse_key_op_delete:
 * @skey: #SeahorseKey to delete
 *
 * Tries to delete the key @skey.
 *
 * Returns: gpgme_error_t
 **/
gpgme_error_t
seahorse_key_op_delete (SeahorseKey *skey)
{
	return op_delete (skey, FALSE);
}

/**
 * seahorse_key_pair_op_delete:
 * @skpair: #SeahorseKeyPair to delete
 *
 * Tries to delete the key pair @skpair.
 *
 * Returns: gpgme_error_t
 **/
gpgme_error_t
seahorse_key_pair_op_delete (SeahorseKeyPair *skpair)
{
	return op_delete (SEAHORSE_KEY (skpair), TRUE);
}

/* Main key edit setup, structure, and a good deal of method content borrowed from gpa */

/* Edit action function */
typedef gpgme_error_t	(*SeahorseEditAction) 	(guint			state,
						 gpointer 		data,
						 gint                   fd);
/* Edit transit function */
typedef guint		(*SeahorseEditTransit)	(guint			current_state,
						 gpgme_status_code_t	status,
						 const gchar		*args,
						 gpointer		data,
						 gpgme_error_t		*err);

/* Edit parameters */
typedef struct
{
	guint 			state;
	gpgme_error_t		err;
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
	parms->err = GPG_OK;
	parms->action = action;
	parms->transit = transit;
	parms->data = data;
	
	return parms;
}

/* Edit callback for gpgme */
static gpgme_error_t
seahorse_key_op_edit (gpointer data, gpgme_status_code_t status,
		      const gchar *args, int fd)
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
	if (GPG_IS_OK (parms->err))
		parms->err = parms->action (parms->state, parms->data, fd);
	
	return parms->err;
}

/* Common edit operation */
static gpgme_error_t
edit_key (SeahorseKey *skey, SeahorseEditParm *parms, SeahorseKeyChange change)
{
    SeahorseKeySource *sksrc;
	gpgme_data_t out;
	gpgme_error_t err;
	
    sksrc = seahorse_key_get_source (skey);
	g_return_val_if_fail (sksrc != NULL, GPG_E (GPG_ERR_INV_KEYRING));
  
	err = gpgme_data_new (&out);
	g_return_val_if_fail (GPG_IS_OK (err), err);
	/* do edit callback, release data */
	err = gpgme_op_edit (sksrc->ctx, skey->key, seahorse_key_op_edit, parms, out);
	gpgme_data_release (out);

	/* signal key */
	if(GPG_IS_OK (err))
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
static gpgme_error_t
sign_action (guint state, gpointer data, int fd)
{
	SignParm *parm = (SignParm*)data;
	
	switch (state) {
		/* select uid */
		case SIGN_UID:
            PRINTF ((fd, "uid %d", parm->index));
			break;
		case SIGN_COMMAND:
            PRINT ((fd, parm->command));
			break;
		/* if expires */
		case SIGN_EXPIRE:
            PRINT ((fd, (parm->expire) ? YES : "N"));
			break;
		case SIGN_CONFIRM:
            PRINT ((fd, YES));
			break;
		case SIGN_CHECK:
            PRINTF ((fd, "%d", parm->check));
			break;
		case SIGN_QUIT:
            PRINT ((fd, QUIT));
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for signing a key */
static guint
sign_transit (guint current_state, gpgme_status_code_t status,
	      const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start state, need to select uid */
		case SIGN_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = SIGN_UID;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
			}
			break;
		/* selected uid, go to command */
		case SIGN_UID:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = SIGN_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
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
            /* if it's already signed then send back an error */
            else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT)) {
                next_state = SIGN_ERROR;
                *err = GPG_E (GPG_ERR_EALREADY);
            /* All other stuff is unexpected */
            } else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
			}
			break;
		/* did expire, go to check */
		case SIGN_EXPIRE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "sign_uid.class"))
				next_state = SIGN_CHECK;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
			}
			break;
		/* did check, go to confirm */
		case SIGN_CHECK:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "sign_uid.okay"))
				next_state = SIGN_CONFIRM;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
			}
			break;
		/* quit, go to confirm to save */
		case SIGN_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = SIGN_CONFIRM;
			else {
				*err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (SIGN_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_op_sign:
 * @skey: #SeahorseKey to sign
 * @index: User ID to sign, 0 is all user IDs
 * @check: #SeahorseSignCheck
 * @options: #SeahorseSignOptions
 *
 * Tries to sign user ID @index of @skey with the default key and the given options.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_op_sign (SeahorseKey *skey, const guint index,
		              SeahorseSignCheck check, SeahorseSignOptions options)
{
	SignParm *sign_parm;
	SeahorseEditParm *parms;
    gpgme_error_t err;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	g_return_val_if_fail (index <= seahorse_key_get_num_uids (skey), GPG_E (GPG_ERR_INV_VALUE));
	
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
	
	err =  edit_key (skey, parms, SKEY_CHANGE_SIGNERS);
 
    /* If it was already signed then it's not an error */
    if (!GPG_IS_OK (err) && gpgme_err_code (err) == GPG_ERR_EALREADY)
        err = GPG_OK;
    
    return err;
}

typedef enum {
	PASS_START,
	PASS_COMMAND,
    PASS_PASSPHRASE,
	PASS_QUIT,
	PASS_SAVE,
	PASS_ERROR
} PassState;

/* action helper for changing passphrase */
static gpgme_error_t
edit_pass_action (guint state, gpointer data, int fd)
{
	switch (state) {
		case PASS_COMMAND:
            PRINT ((fd, "passwd"));
			break;
        case PASS_PASSPHRASE:
            /* Do nothing */
            return GPG_OK;
		case PASS_QUIT:
            PRINT ((fd, QUIT));
			break;
		case PASS_SAVE:
            PRINT ((fd, YES));
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for changing passphrase */
static guint
edit_pass_transit (guint current_state, gpgme_status_code_t status,
		   const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start state, go to command */
		case PASS_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = PASS_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PASS_ERROR);
			}
			break;
		/* did command, go to should be the passphrase now */
		case PASS_COMMAND:
            if (status == GPGME_STATUS_NEED_PASSPHRASE_SYM)
    			next_state = PASS_PASSPHRASE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PASS_ERROR);
          }
          break;
        /* got passphrase now quit */
        case PASS_PASSPHRASE:
            if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
             next_state = PASS_QUIT;
         else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PASS_ERROR);
           }
          break;
        
		/* quit, go to save */
		case PASS_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = PASS_SAVE;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PASS_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PASS_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_pair_op_change_pass:
 * @skpair: #SeahorseKeyPair whose passphrase to change
 *
 * Tries to change the passphrase of @skpair. 
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_pair_op_change_pass (SeahorseKeyPair *skpair)
{
	SeahorseEditParm *parms;
	gpgme_error_t err;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	
	parms = seahorse_edit_parm_new (PASS_START, edit_pass_action, edit_pass_transit, NULL);
	
	err = edit_key (SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_PASS);
	
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
static gpgme_error_t
edit_trust_action (guint state, gpointer data, int fd)
{
	SeahorseValidity trust = (SeahorseValidity)data;
	
	switch (state) {
		/* enter command */
		case TRUST_COMMAND:
            PRINT ((fd, "trust"));
			break;
		/* enter numeric trust value */
		case TRUST_VALUE:
            PRINTF ((fd, "%d", trust));
			break;
		/* confirm ultimate or if save */
		case TRUST_CONFIRM:
            PRINT ((fd, YES));
			break;
		/* quit */
		case TRUST_QUIT:
            PRINT ((fd, QUIT));
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for setting trust of a key */
static guint
edit_trust_transit (guint current_state, gpgme_status_code_t status,
		    const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start state */
		case TRUST_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
			}
			break;
		/* did command, next is value */
		case TRUST_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "edit_ownertrust.value"))
				next_state = TRUST_VALUE;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
			}
			break;
		/* did value, go to quit or confirm ultimate */
		case TRUST_VALUE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_QUIT;
			else if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "edit_ownertrust.set_ultimate.okay"))
				next_state = TRUST_CONFIRM;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
			}
			break;
		/* did confirm ultimate, go to quit */
		case TRUST_CONFIRM:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = TRUST_QUIT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
			}
			break;
		/* did quit, go to confirm to finish op */
		case TRUST_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = TRUST_CONFIRM;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (TRUST_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_op_set_trust:
 * @skey: #SeahorseKey whose trust will be changed
 * @trust: New trust value that must be at least SEAHORSE_VALIDITY_UNKNOWN.
 * If @skey is a #SeahorseKeyPair, then @trust cannot be SEAHORSE_VALIDITY_UNKNOWN.
 * If @skey is not a #SeahorseKeyPair, then @trust cannot be SEAHORSE_VALIDITY_ULTIMATE.
 *
 * Tries to change the owner trust of @skey to @trust.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_op_set_trust (SeahorseKey *skey, SeahorseValidity trust)
{
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	g_return_val_if_fail (trust >= GPGME_VALIDITY_UNKNOWN, GPG_E (GPG_ERR_INV_VALUE));
	g_return_val_if_fail (seahorse_key_get_trust (skey) != trust, GPG_E (GPG_ERR_INV_VALUE));
	
	if (SEAHORSE_IS_KEY_PAIR (skey))
		g_return_val_if_fail (trust != SEAHORSE_VALIDITY_UNKNOWN, GPG_E (GPG_ERR_INV_VALUE));
	else
		g_return_val_if_fail (trust != SEAHORSE_VALIDITY_ULTIMATE, GPG_E (GPG_ERR_INV_VALUE));
	
	parms = seahorse_edit_parm_new (TRUST_START, edit_trust_action,
		edit_trust_transit, (gpointer)trust);
	
	return edit_key (skey, parms, SKEY_CHANGE_TRUST);
}

typedef enum {
	DISABLE_START,
	DISABLE_COMMAND,
	DISABLE_QUIT,
	DISABLE_ERROR
} DisableState;

/* action helper for disable/enable a key */
static gpgme_error_t
edit_disable_action (guint state, gpointer data, int fd)
{
	const gchar *command = data;
	
	switch (state) {
		case DISABLE_COMMAND:
            PRINT ((fd, command));
			break;
		case DISABLE_QUIT:
            PRINT ((fd, QUIT));
			break;
		default:
			break;
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for disable/enable a key */
static guint
edit_disable_transit (guint current_state, gpgme_status_code_t status,
		      const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, do command */
		case DISABLE_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DISABLE_ERROR);
			}
			break;
		/* did command, quit */
		case DISABLE_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_QUIT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DISABLE_ERROR);
			}
		/* error, quit */
		case DISABLE_ERROR:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DISABLE_QUIT;
			else
				next_state = DISABLE_ERROR;
			break;
		default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DISABLE_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_op_set_disabled:
 * @skey: #SeahorseKey to change
 * @disabled: New disabled state
 *
 * Tries to change disabled state of @skey to @disabled.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_op_set_disabled (SeahorseKey *skey, gboolean disabled)
{
	gchar *command;
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	/* Make sure changing disabled */
	g_return_val_if_fail (disabled != skey->key->disabled, GPG_E (GPG_ERR_INV_VALUE));
	/* Get command and op */
	if (disabled)
		command = "disable";
	else
		command = "enable";
	
	parms = seahorse_edit_parm_new (DISABLE_START, edit_disable_action, edit_disable_transit, command);
	
	return edit_key (skey, parms, SKEY_CHANGE_DISABLED);
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
static gpgme_error_t
edit_expire_action (guint state, gpointer data, int fd)
{
	ExpireParm *parm = (ExpireParm*)data;
  
	switch (state) {
		/* selected key */
		case EXPIRE_SELECT:
            PRINTF ((fd, "key %d", parm->index));
			break;
		case EXPIRE_COMMAND:
            PRINT ((fd, "expire"));
			break;
		/* set date */
		case EXPIRE_DATE:
            PRINT ((fd, (parm->expires) ?
				seahorse_util_get_date_string (parm->expires) : "0"));
			break;
		case EXPIRE_QUIT:
            PRINT ((fd, QUIT));
			break;
		case EXPIRE_SAVE:
            PRINT ((fd, YES));
			break;
		case EXPIRE_ERROR:
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}

    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for changing expiration date of a key */
static guint
edit_expire_transit (guint current_state, gpgme_status_code_t status,
		     const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
 
	switch (current_state) {
		/* start state, selected key */
		case EXPIRE_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_SELECT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
			}
			break;
		/* selected key, do command */
		case EXPIRE_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
			}
			break;
		/* did command, set expires */
		case EXPIRE_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.valid"))
				next_state = EXPIRE_DATE;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
			}
			break;
		/* set expires, quit */
		case EXPIRE_DATE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = EXPIRE_QUIT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
			}
			break;
		/* quit, save */
		case EXPIRE_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = EXPIRE_SAVE;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (EXPIRE_ERROR);
			break;
	}
	return next_state;
}

/**
 * seahorse_key_pair_op_set_expires:
 * @skpair: #SeahorseKeyPair whose expiration date to change
 * @index: Index of key to change, 0 being the primary key
 * @expires: New expiration date, 0 being never
 *
 * Tries to change the expiration date of the key at @index of @skpair to @expires.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_pair_op_set_expires (SeahorseKeyPair *skpair,
				  const guint index, const time_t expires)
{
	ExpireParm *exp_parm;
	SeahorseEditParm *parms;
	SeahorseKey *skey;
	gpgme_subkey_t subkey;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	skey = SEAHORSE_KEY (skpair);
	subkey = seahorse_key_get_nth_subkey (skey, index);

	/* Make sure changing expires */
	g_return_val_if_fail (subkey != NULL && expires != subkey->expires, FALSE);
	
	exp_parm = g_new (ExpireParm, 1);
	exp_parm->index = index;
	exp_parm->expires = expires;
	
	parms = seahorse_edit_parm_new (EXPIRE_START, edit_expire_action, edit_expire_transit, exp_parm);
	
	return edit_key (skey, parms, SKEY_CHANGE_EXPIRES);
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
static gpgme_error_t
add_revoker_action (guint state, gpointer data, int fd)
{
	gchar *keyid = (gchar*)data;
	
	switch (state) {
		case ADD_REVOKER_COMMAND:
            PRINT ((fd, "addrevoker"));
			break;
		/* select revoker */
		case ADD_REVOKER_SELECT:
            PRINT ((fd, keyid));
			break;
		case ADD_REVOKER_CONFIRM:
            PRINT ((fd, YES));
			break;
		case ADD_REVOKER_QUIT:
            PRINT ((fd, QUIT));
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for adding a revoker */
static guint
add_revoker_transit (guint current_state, gpgme_status_code_t status,
		     const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, do command */
		case ADD_REVOKER_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_REVOKER_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
			}
			break;
		/* did command, select revoker */
		case ADD_REVOKER_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keyedit.add_revoker"))
				next_state = ADD_REVOKER_SELECT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
			}
			break;
		/* selected revoker, confirm */
		case ADD_REVOKER_SELECT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "keyedit.add_revoker.okay"))
				next_state = ADD_REVOKER_CONFIRM;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
			}
			break;
		/* confirmed, quit */
		case ADD_REVOKER_CONFIRM:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_REVOKER_QUIT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
			}
			break;
		/* quit, confirm(=save) */
		case ADD_REVOKER_QUIT:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
				next_state = ADD_REVOKER_CONFIRM;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (ADD_REVOKER_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_pair_op_add_revoker:
 * @skpair: #SeahorseKeyPair to add a revoker to
 *
 * Tries to add the default key as a revoker for @skpair.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_pair_op_add_revoker (SeahorseKeyPair *skpair, SeahorseKeyPair *revoker)
{
	SeahorseEditParm *parms;
	
    g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (revoker), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	
	parms = seahorse_edit_parm_new (ADD_REVOKER_START, add_revoker_action,
		add_revoker_transit, (gpointer)seahorse_key_get_id (SEAHORSE_KEY (revoker)->key));
	
	return edit_key (SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_REVOKERS);
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
static gpgme_error_t
add_uid_action (guint state, gpointer data, int fd)
{
	UidParm *parm = (UidParm*)data;
	
	switch (state) {
		case ADD_UID_COMMAND:
            PRINT ((fd, "adduid"));
			break;
		case ADD_UID_NAME:
            PRINT ((fd, parm->name));
			break;
		case ADD_UID_EMAIL:
            PRINT ((fd, parm->email));
			break;
		case ADD_UID_COMMENT:
            PRINT ((fd, parm->comment));
			break;
		case ADD_UID_QUIT:
            PRINT ((fd, QUIT));
			break;
		case ADD_UID_SAVE:
            PRINT ((fd, YES));
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for adding a new user ID */
static guint
add_uid_transit (guint current_state, gpgme_status_code_t status,
		 const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, do command */
		case ADD_UID_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_UID_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_UID_ERROR);
			}
			break;
		/* did command, do name */
		case ADD_UID_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.name"))
				next_state = ADD_UID_NAME;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_UID_ERROR);
			}
			break;
		/* did name, do email */
		case ADD_UID_NAME:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.email"))
				next_state = ADD_UID_EMAIL;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_UID_ERROR);
			}
			break;
		/* did email, do comment */
		case ADD_UID_EMAIL:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.comment"))
				next_state = ADD_UID_COMMENT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_UID_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_UID_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (ADD_UID_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_pair_op_add_uid:
 * @skpair: #SeahorseKeyPair to add a user ID to
 * @name: New user ID name. Must be at least 5 characters long
 * @email: Optional email address
 * @comment: Optional comment
 *
 * Tries to add a new user ID to @skpair with the given @name, @email, and @comment.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_pair_op_add_uid (SeahorseKeyPair *skpair, const gchar *name, 
                              const gchar *email, const gchar *comment)
{
	SeahorseEditParm *parms;
	UidParm *uid_parm;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	g_return_val_if_fail (name != NULL && strlen (name) >= 5, GPG_E (GPG_ERR_INV_VALUE));
	
	uid_parm = g_new (UidParm, 1);
	uid_parm->name = name;
	uid_parm->email = email;
	uid_parm->comment = comment;
	
	parms = seahorse_edit_parm_new (ADD_UID_START, add_uid_action, add_uid_transit, uid_parm);
	
	return edit_key (SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_UIDS);
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
static gpgme_error_t
add_key_action (guint state, gpointer data, int fd)
{
	SubkeyParm *parm = (SubkeyParm*)data;
	
	switch (state) {
		case ADD_KEY_COMMAND:
            PRINT ((fd, "addkey"));
			break;
		case ADD_KEY_TYPE:
            PRINTF ((fd, "%d", parm->type));
			break;
		case ADD_KEY_LENGTH:
            PRINTF ((fd, "%d", parm->length));
			break;
		/* Get exact date or 0 */
		case ADD_KEY_EXPIRES:
            PRINT ((fd, (parm->expires) ?
				seahorse_util_get_date_string (parm->expires) : "0"));
			break;
		case ADD_KEY_QUIT:
            PRINT ((fd, QUIT));
			break;
		case ADD_KEY_SAVE:
            PRINT ((fd, YES));
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for adding a subkey */
static guint
add_key_transit (guint current_state, gpgme_status_code_t status,
		 const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;

	switch (current_state) {
		/* start, do command */
		case ADD_KEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = ADD_KEY_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_KEY_ERROR);
			}
			break;
		/* did command, do type */
		case ADD_KEY_COMMAND:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.algo"))
				next_state = ADD_KEY_TYPE;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_KEY_ERROR);
			}
			break;
		/* did type, do length */
		case ADD_KEY_TYPE:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.size"))
				next_state = ADD_KEY_LENGTH;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_KEY_ERROR);
			}
			break;
		/* did length, do expires */
		case ADD_KEY_LENGTH:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "keygen.valid"))
				next_state = ADD_KEY_EXPIRES;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_KEY_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_KEY_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (ADD_KEY_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_pair_op_add_subkey:
 * @skpair: #SeahorseKeyPair to add a subkey to
 * @type: #SeahorseKeyType of new subkey, must be DSA, ELGAMAL, or an RSA type
 * @length: Length of new subkey, must be within #SeahorseKeyLength ranges for @type
 * @expires: Expiration date of new subkey, 0 being never
 *
 * Tries to add a new subkey to @skpair given @type, @length, and @expires.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_pair_op_add_subkey (SeahorseKeyPair *skpair, const SeahorseKeyType type, 
                                 const guint length, const time_t expires)
{
	SeahorseEditParm *parms;
	SubkeyParm *key_parm;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY_PAIR (skpair), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	
	/* Check length range & type */
	switch (type) {
		case DSA:
			g_return_val_if_fail (length >= DSA_MIN && length <= DSA_MAX, GPG_E (GPG_ERR_INV_VALUE));
			break;
		case ELGAMAL:
			g_return_val_if_fail (length >= ELGAMAL_MIN && length <= LENGTH_MAX, GPG_E (GPG_ERR_INV_VALUE));
			break;
		case RSA_SIGN: case RSA_ENCRYPT:
			g_return_val_if_fail (length >= RSA_MIN && length <= LENGTH_MAX, GPG_E (GPG_ERR_INV_VALUE));
			break;
		default:
			g_return_val_if_reached (GPG_E (GPG_ERR_INV_VALUE));
			break;
	}
	
	key_parm = g_new (SubkeyParm, 1);
	key_parm->type = type;
	key_parm->length = length;
	key_parm->expires = expires;
	
	parms = seahorse_edit_parm_new (ADD_KEY_START, add_key_action, add_key_transit, key_parm);
	
	return edit_key (SEAHORSE_KEY (skpair), parms, SKEY_CHANGE_SUBKEYS);
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
static gpgme_error_t
del_key_action (guint state, gpointer data, int fd)
{
	switch (state) {
		/* select key */
		case DEL_KEY_SELECT:
            PRINTF ((fd, "key %d", (guint)data));
			break;
		case DEL_KEY_COMMAND:
            PRINT ((fd, "delkey"));
			break;
		case DEL_KEY_CONFIRM:
            PRINT ((fd, YES));
			break;
		case DEL_KEY_QUIT:
            PRINT ((fd, QUIT));
			break;
		default:
			return GPG_E (GPG_ERR_GENERAL);
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for deleting a subkey */
static guint
del_key_transit (guint current_state, gpgme_status_code_t status,
		 const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;

	switch (current_state) {
		/* start, select key */
		case DEL_KEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_SELECT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DEL_KEY_ERROR);
			}
			break;
		/* selected key, do command */
		case DEL_KEY_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = DEL_KEY_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DEL_KEY_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DEL_KEY_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DEL_KEY_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DEL_KEY_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_op_del_subkey:
 * @skey: #SeahorseKey whose subkey to delete
 * @index: Index of subkey to delete, must be at least 1
 *
 * Tries to delete subkey @index from @skey.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_op_del_subkey (SeahorseKey *skey, const guint index)
{
	SeahorseEditParm *parms;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_subkeys (skey), GPG_E (GPG_ERR_INV_VALUE));
	
	parms = seahorse_edit_parm_new (DEL_KEY_START, del_key_action,
		del_key_transit, (gpointer)index);
	
	return edit_key (skey, parms, SKEY_CHANGE_SUBKEYS);
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
static gpgme_error_t
rev_subkey_action (guint state, gpointer data, int fd)
{
	RevSubkeyParm *parm = (RevSubkeyParm*)data;
	
	switch (state) {
		case REV_SUBKEY_SELECT:
            PRINTF ((fd, "key %d", parm->index));
			break;
		case REV_SUBKEY_COMMAND:
            PRINT ((fd, "revkey"));
			break;
		case REV_SUBKEY_CONFIRM:
            PRINT ((fd, YES));
			break;
		case REV_SUBKEY_REASON:
            PRINTF ((fd, "%d", parm->reason));
			break;
		case REV_SUBKEY_DESCRIPTION:
            PRINTF ((fd, "%s\n", parm->description));
			break;
		/* Need empty line */
		case REV_SUBKEY_ENDDESC:
            PRINT ((fd, "\n"));
			break;
		case REV_SUBKEY_QUIT:
            PRINT ((fd, QUIT));
			break;
		default:
			g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
	}
	
    PRINT ((fd, "\n"));
	return GPG_OK;
}

/* transition helper for revoking a subkey */
static guint
rev_subkey_transit (guint current_state, gpgme_status_code_t status,
		    const gchar *args, gpointer data, gpgme_error_t *err)
{
	guint next_state;
	
	switch (current_state) {
		/* start, select key */
		case REV_SUBKEY_START:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_SELECT;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
			}
			break;
		/* selected key, do command */
		case REV_SUBKEY_SELECT:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
				next_state = REV_SUBKEY_COMMAND;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
			}
			break;
		/* did command, confirm */
		case REV_SUBKEY_COMMAND:
			if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "keyedit.revoke.subkey.okay"))
				next_state = REV_SUBKEY_CONFIRM;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
			}
			break;
		/* did reason, do description */
		case REV_SUBKEY_REASON:
			if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, "ask_revocation_reason.text"))
				next_state = REV_SUBKEY_DESCRIPTION;
			else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
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
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
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
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (REV_SUBKEY_ERROR);
			break;
	}
	
	return next_state;
}

/**
 * seahorse_key_op_revoke_subkey:
 * @skey: #SeahorseKey whose subkey to revoke
 * @index: Index of subkey to revoke, must be at least 1
 * @reason: #SeahorseRevokeReason for revoking the key
 * @description: Optional description of revocation
 *
 * Tries to revoke subkey @index of @skey given @reason and @description.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_key_op_revoke_subkey (SeahorseKey *skey, const guint index,
			                   SeahorseRevokeReason reason, const gchar *description)
{
	RevSubkeyParm *rev_parm;
	SeahorseEditParm *parms;
	gpgme_subkey_t subkey;
	
	g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
	/* Check index range */
	g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_subkeys (skey), GPG_E (GPG_ERR_INV_VALUE));
	/* Make sure not revoked */
	subkey = seahorse_key_get_nth_subkey (skey, index);
	g_return_val_if_fail (subkey != NULL && !subkey->revoked, GPG_E (GPG_ERR_INV_VALUE));
	
	rev_parm = g_new0 (RevSubkeyParm, 1);
	rev_parm->index = index;
	rev_parm->reason = reason;
	rev_parm->description = description;
	
	parms = seahorse_edit_parm_new (REV_SUBKEY_START, rev_subkey_action,
		rev_subkey_transit, rev_parm);
	
	return edit_key (skey, parms, SKEY_CHANGE_SUBKEYS);
}

typedef struct {
    guint           index;
} PrimaryParm;

typedef enum {
    PRIMARY_START,
    PRIMARY_SELECT,
    PRIMARY_COMMAND,
    PRIMARY_QUIT,
    PRIMARY_SAVE,
    PRIMARY_ERROR
} PrimaryState;

/* action helper for setting primary uid */
static gpgme_error_t
primary_action (guint state, gpointer data, int fd)
{
    PrimaryParm *parm = (PrimaryParm*)data;
    
    switch (state) {
    case PRIMARY_SELECT:
        /* Note that the GPG id is not 0 based */
        PRINTF ((fd, "uid %d", parm->index));
        break;
    case PRIMARY_COMMAND:
        PRINT ((fd, "primary"));
        break;
    case PRIMARY_QUIT:
        PRINT ((fd, QUIT));
        break;
    case PRIMARY_SAVE:
        PRINT ((fd, YES));
        break;
    default:
        g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
        break;
    }
  
    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for setting primary key */
static guint
primary_transit (guint current_state, gpgme_status_code_t status,
            const gchar *args, gpointer data, gpgme_error_t *err)
{
    guint next_state;
  
    switch (current_state) {
    
    /* start, select key */
    case PRIMARY_START:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = PRIMARY_SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;

    /* selected key, do command */
    case PRIMARY_SELECT:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = PRIMARY_COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;

    /* did command, quit */
    case PRIMARY_COMMAND:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = PRIMARY_QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;
        
    /* quitting so save */
    case PRIMARY_QUIT:        
        if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
            next_state = PRIMARY_SAVE;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;

    /* error, quit */
    case PRIMARY_ERROR:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = PRIMARY_QUIT;
        else
            next_state = PRIMARY_ERROR;
        break;
        
    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (PRIMARY_ERROR);
        break;
    }
    
    return next_state;
}
                
gpgme_error_t   
seahorse_key_op_primary_uid (SeahorseKey *skey, const guint index)
{
    PrimaryParm *pri_parm;
    SeahorseEditParm *parms;
    gpgme_user_id_t uid;
 
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    
    /* Check index range */
    g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_uids (skey), GPG_E (GPG_ERR_INV_VALUE));

    /* Make sure not revoked */
    uid = seahorse_key_get_nth_userid (skey, index - 1);
    g_return_val_if_fail (uid != NULL && !uid->revoked && !uid->invalid, 
                            GPG_E (GPG_ERR_INV_VALUE));
  
    pri_parm = g_new0 (PrimaryParm, 1);
    pri_parm->index = index;
   
    parms = seahorse_edit_parm_new (PRIMARY_START, primary_action,
                primary_transit, pri_parm);
 
    return edit_key (skey, parms, SKEY_CHANGE_UIDS);    
}


typedef struct {
    guint           index;
} DelUidParm;

typedef enum {
    DEL_UID_START,
    DEL_UID_SELECT,
    DEL_UID_COMMAND,
    DEL_UID_CONFIRM,
    DEL_UID_QUIT,
    DEL_UID_SAVE,
    DEL_UID_ERROR
} DelUidState;

/* action helper for removing a uid */
static gpgme_error_t
del_uid_action (guint state, gpointer data, int fd)
{
    DelUidParm *parm = (DelUidParm*)data;
    
    switch (state) {
    case DEL_UID_SELECT:
        PRINTF ((fd, "uid %d", parm->index));
        break;
    case DEL_UID_COMMAND:
        PRINT ((fd, "deluid"));
        break;
    case DEL_UID_CONFIRM:
        PRINT ((fd, YES));
        break;
    case DEL_UID_QUIT:
        PRINT ((fd, QUIT));
        break;
    case DEL_UID_SAVE:
        PRINT ((fd, YES));
        break;
    default:
        g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
        break;
    }
  
    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for setting deleting a uid */
static guint
del_uid_transit (guint current_state, gpgme_status_code_t status,
            const gchar *args, gpointer data, gpgme_error_t *err)
{
    guint next_state;
  
    switch (current_state) {
    
    /* start, select key */
    case DEL_UID_START:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = DEL_UID_SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DEL_UID_ERROR);
        }
        break;

    /* selected key, do command */
    case DEL_UID_SELECT:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = DEL_UID_COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DEL_UID_ERROR);
        }
        break;

    /* did command, confirm */
    case DEL_UID_COMMAND:
        if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, "keyedit.remove.uid.okay"))
            next_state = DEL_UID_CONFIRM;
        else if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = DEL_UID_QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (REV_SUBKEY_ERROR);
        }
        break;

    /* confirmed, quit */
    case DEL_UID_CONFIRM:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = DEL_UID_QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DEL_UID_ERROR);
        }
        break;
                
    /* quitted so save */
    case DEL_UID_QUIT:        
        if (status == GPGME_STATUS_GET_BOOL && g_str_equal (args, SAVE))
            next_state = DEL_UID_SAVE;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (REV_SUBKEY_ERROR);
        }
        break;

    /* error, quit */
    case DEL_UID_ERROR:
        if (status == GPGME_STATUS_GET_LINE && g_str_equal (args, PROMPT))
            next_state = DEL_UID_QUIT;
        else
            next_state = DEL_UID_ERROR;
        break;
        
    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (DEL_UID_ERROR);
        break;
    }
    
    return next_state;
}
                
gpgme_error_t   
seahorse_key_op_del_uid (SeahorseKey *skey, const guint index)
{
    DelUidParm *del_uid_parm;
    SeahorseEditParm *parms;
    gpgme_user_id_t uid;
 
    g_return_val_if_fail (SEAHORSE_IS_KEY (skey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    
    /* Check index range */
    g_return_val_if_fail (index >= 1 && index <= seahorse_key_get_num_uids (skey), GPG_E (GPG_ERR_INV_VALUE));

    /* Make sure not revoked */
    uid = seahorse_key_get_nth_userid (skey, index - 1);
    g_return_val_if_fail (uid != NULL && !uid->revoked && !uid->invalid, 
                            GPG_E (GPG_ERR_INV_VALUE));
  
    del_uid_parm = g_new0 (DelUidParm, 1);
    del_uid_parm->index = index;
   
    parms = seahorse_edit_parm_new (DEL_UID_START, del_uid_action,
                del_uid_transit, del_uid_parm);
 
    return edit_key (skey, parms, SKEY_CHANGE_UIDS);    
}
