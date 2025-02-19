/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004 Stefan Walter
 * Copyright (C) 2005 Adam Schreiber
 * Copyright (C) 2011 Collabora Ltd.
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

#include "seahorse-gpgme-key-op.h"

#include "seahorse-gpgme.h"
#include "seahorse-gpgme-data.h"
#include "seahorse-gpg-op.h"

#include "libseahorse/seahorse-progress.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Common values of the "args" arg */
#define PROMPT "keyedit.prompt"
#define QUIT "quit"
#define SAVE "keyedit.save.okay"
#define YES "Y"
#define NO "N"

#define PRINT(args)  if(!seahorse_util_print_fd args) return GPG_E (GPG_ERR_GENERAL)
#define PRINTF(args) if(!seahorse_util_printf_fd args) return GPG_E (GPG_ERR_GENERAL)

#define GPG_UNKNOWN     1
#define GPG_NEVER       2
#define GPG_MARGINAL    3
#define GPG_FULL        4
#define GPG_ULTIMATE    5

static void
on_key_op_progress (void *opaque,
                    const char *what,
                    int type,
                    int current,
                    int total)
{
    GTask *task = G_TASK (opaque);
    seahorse_progress_update (g_task_get_cancellable (task), task, "%s", what);
}

static gboolean
on_key_op_generate_complete (gpgme_error_t gerr,
                             gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    g_autoptr(GError) error = NULL;

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return G_SOURCE_REMOVE;
    }

    seahorse_progress_end (g_task_get_cancellable (task), task);
    g_task_return_boolean (task, TRUE);
    return G_SOURCE_REMOVE;
}

/**
 * Tries to generate a new key based on the given parameters.
 */
void
seahorse_gpgme_key_op_generate_async (SeahorseGpgmeKeyring  *keyring,
                                      SeahorseGpgmeKeyParms *parms,
                                      GCancellable          *cancellable,
                                      GAsyncReadyCallback    callback,
                                      void                  *user_data)
{
    gpgme_ctx_t gctx;
    g_autoptr(GTask) task = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree char *parms_str = NULL;
    gpgme_error_t gerr = 0;
    g_autoptr(GSource) gsource = NULL;

    g_return_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring));

    parms_str = seahorse_gpgme_key_parms_to_string (parms);

    gctx = seahorse_gpgme_keyring_new_context (&gerr);

    task = g_task_new (keyring, cancellable, callback, user_data);
    gpgme_set_progress_cb (gctx, on_key_op_progress, task);
    g_task_set_task_data (task, gctx, (GDestroyNotify) gpgme_release);

    seahorse_progress_prep_and_begin (cancellable, task, NULL);
    gsource = seahorse_gpgme_gsource_new (gctx, cancellable);
    g_source_set_callback (gsource, (GSourceFunc)on_key_op_generate_complete,
                           g_object_ref (task), g_object_unref);

    if (gerr == 0)
        gerr = gpgme_op_genkey_start (gctx, parms_str, NULL, NULL);

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_source_attach (gsource, g_main_context_default ());
}

gboolean
seahorse_gpgme_key_op_generate_finish (SeahorseGpgmeKeyring *keyring,
                                       GAsyncResult *result,
                                       GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, keyring), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

/* helper function for deleting @skey */
static gpgme_error_t
op_delete (SeahorseGpgmeKey *pkey, gboolean secret)
{
    SeahorseGpgmeKeyring *keyring;
    gpgme_error_t gerr;
    gpgme_key_t key;
    gpgme_ctx_t ctx;

    keyring = SEAHORSE_GPGME_KEYRING (seahorse_item_get_place (SEAHORSE_ITEM (pkey)));
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), GPG_E (GPG_ERR_INV_KEYRING));

    g_object_ref (pkey);

    seahorse_util_wait_until ((key = seahorse_gpgme_key_get_public (pkey)) != NULL);

    ctx = seahorse_gpgme_keyring_new_context (&gerr);
    if (ctx == NULL)
        return gerr;

    gerr = gpgme_op_delete (ctx, key, secret);
    if (GPG_IS_OK (gerr))
        seahorse_gpgme_keyring_remove_key (keyring, SEAHORSE_GPGME_KEY (pkey));

    gpgme_release (ctx);
    g_object_unref (pkey);
    return gerr;
}

gpgme_error_t
seahorse_gpgme_key_op_delete (SeahorseGpgmeKey *pkey)
{
    return op_delete (pkey, FALSE);
}


gpgme_error_t
seahorse_gpgme_key_op_delete_pair (SeahorseGpgmeKey *pkey)
{
    return op_delete (pkey, TRUE);
}

/* Main key edit setup, structure, and a good deal of method content borrowed from gpa */

/* Edit action function */
typedef gpgme_error_t (*SeahorseEditAction) (unsigned int  state,
                                             void         *data,
                                             int           fd);
/* Edit transit function */
typedef unsigned int (*SeahorseEditTransit) (unsigned int   current_state,
                                             const char    *status,
                                             const char    *args,
                                             void          *data,
                                             gpgme_error_t *err);

/* Edit parameters */
typedef struct
{
    unsigned int         state;
    gpgme_error_t        err;
    SeahorseEditAction   action;
    SeahorseEditTransit  transit;
    void                *data;
} SeahorseEditParm;

/* Creates new edit parameters with defaults */
static SeahorseEditParm*
seahorse_edit_parm_new (unsigned int         state,
                        SeahorseEditAction   action,
                        SeahorseEditTransit  transit,
                        void                *data)
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
seahorse_gpgme_key_op_interact (void       *data,
                                const char *status,
                                const char *args,
                                int         fd)
{
    SeahorseEditParm *parms = (SeahorseEditParm *) data;
    static const char *NO_INTERACTION_STATUSES[] = {
        "EOF", "GOT_IT", "NEED_PASSPHRASE", "GOOD_PASSPHRASE", "BAD_PASSPHRASE",
        "USERID_HINT", "SIGEXPIRED", "KEYEXPIRED", "PROGRESS", "KEY_CREATED",
        "ALREADY_SIGNED", "MISSING_PASSPHRASE", "KEY_CONSIDERED",
        "PINENTRY_LAUNCHED",
        NULL
    };

    g_return_val_if_fail (status, parms->err);

    g_debug ("[edit key] state: %d / status: '%s' / args: '%s'",
             parms->state, status, args);

    /* An empty string represents EOF */
    if (!status[0])
        return parms->err;

    /* Ignore these status lines, as they don't require any response */
    if (g_strv_contains (NO_INTERACTION_STATUSES, status))
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
edit_gpgme_key (gpgme_ctx_t       ctx,
                gpgme_key_t       key,
                SeahorseEditParm *parms)
{
    gboolean own_context = FALSE;
    gpgme_data_t out;
    gpgme_error_t gerr;

    g_assert (key);
    g_assert (parms);

    gpgme_key_ref (key);

    if (ctx == NULL) {
        ctx = seahorse_gpgme_keyring_new_context (&gerr);
        if (ctx == NULL)
            return gerr;
        own_context = TRUE;
    }

    out = seahorse_gpgme_data_new ();

    /* do edit callback, release data */
    gerr = gpgme_op_interact (ctx, key, 0, seahorse_gpgme_key_op_interact, parms, out);

    if (gpgme_err_code (gerr) == GPG_ERR_BAD_PASSPHRASE) {
        seahorse_util_show_error(NULL, _("Wrong password"), _("This was the third time you entered a wrong password. Please try again."));
    }

    seahorse_gpgme_data_release (out);
    if (own_context)
        gpgme_release (ctx);
    gpgme_key_unref (key);
    return gerr;
}

static gpgme_error_t
edit_refresh_gpgme_key (gpgme_ctx_t ctx, gpgme_key_t key, SeahorseEditParm *parms)
{
    gpgme_error_t gerr;

    gerr = edit_gpgme_key (ctx, key, parms);
    if (GPG_IS_OK (gerr))
        seahorse_gpgme_key_refresh_matching (key);

    return gerr;
}

static gpgme_error_t
edit_key (SeahorseGpgmeKey *pkey, SeahorseEditParm *parms)
{
    SeahorseGpgmeKeyring *keyring;
    gpgme_error_t gerr;
    gpgme_key_t key;
    gpgme_ctx_t ctx;

    keyring = SEAHORSE_GPGME_KEYRING (seahorse_item_get_place (SEAHORSE_ITEM (pkey)));
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), GPG_E (GPG_ERR_INV_KEYRING));

    g_object_ref (pkey);

    seahorse_util_wait_until ((key = seahorse_gpgme_key_get_public (pkey)) != NULL);

    ctx = seahorse_gpgme_keyring_new_context (&gerr);
    if (ctx != NULL) {
        gerr = edit_refresh_gpgme_key (ctx, key, parms);
        gpgme_release (ctx);
    }

    g_object_unref (pkey);
    return gerr;
}

typedef struct
{
    unsigned int       index;
    char              *command;
    gboolean           expire;
    SeahorseSignCheck  check;
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
sign_action (unsigned int  state,
             void         *data,
             int           fd)
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
static unsigned int
sign_transit (unsigned int   current_state,
              const char    *status,
              const char    *args,
              void          *data,
              gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {
        /* start state, need to select uid */
        case SIGN_START:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = SIGN_UID;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
            }
            break;
        /* selected uid, go to command */
        case SIGN_UID:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = SIGN_COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
            }
            break;
        case SIGN_COMMAND:
            /* if doing all uids, confirm */
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "keyedit.sign_all.okay"))
                next_state = SIGN_CONFIRM;
            else if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "sign_uid.okay"))
                next_state = SIGN_CONFIRM;
            /* need to do expires */
            else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "sign_uid.expire"))
                next_state = SIGN_EXPIRE;
            /*  need to do check */
            else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "sign_uid.class"))
                next_state = SIGN_CHECK;
            /* if it's already signed then send back an error */
            else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT)) {
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
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "sign_uid.class"))
                next_state = SIGN_CHECK;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
            }
            break;
        case SIGN_CONFIRM:
            /* need to do check */
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "sign_uid.class"))
                next_state = SIGN_CHECK;
            else if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "sign_uid.okay"))
                next_state = SIGN_CONFIRM;
            /* need to do expire */
            else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "sign_uid.expire"))
                next_state = SIGN_EXPIRE;
            /* quit */
            else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = SIGN_QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
            }
            break;
        /* did check, go to confirm */
        case SIGN_CHECK:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "sign_uid.okay"))
                next_state = SIGN_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
            }
            break;
        /* quit, go to confirm to save */
        case SIGN_QUIT:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
                next_state = SIGN_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SIGN_ERROR);
            }
            break;
        /* error, go to quit */
        case SIGN_ERROR:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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

static gpgme_error_t
sign_process (gpgme_key_t         signed_key,
              gpgme_key_t         signing_key,
              unsigned int        sign_index,
              SeahorseSignCheck   check,
              SeahorseSignOptions options)
{
    SeahorseEditParm *parms;
    SignParm sign_parm;
    gpgme_ctx_t ctx;
    gpgme_error_t gerr;

    ctx = seahorse_gpgme_keyring_new_context (&gerr);
    if (ctx == NULL)
        return gerr;

    gerr = gpgme_signers_add (ctx, signing_key);
    if (!GPG_IS_OK (gerr))
        return gerr;

    sign_parm.index = sign_index;
    sign_parm.expire = ((options & SIGN_EXPIRES) != 0);
    sign_parm.check = check;
    sign_parm.command = g_strdup_printf ("%s%ssign",
                                         (options & SIGN_NO_REVOKE) ? "nr" : "",
                                         (options & SIGN_LOCAL) ? "l" : "");

    parms = seahorse_edit_parm_new (SIGN_START, sign_action, sign_transit, &sign_parm);

    gerr =  edit_refresh_gpgme_key (ctx, signed_key, parms);
    g_free (sign_parm.command);
    g_free (parms);

    gpgme_release (ctx);

    return gerr;
}

gpgme_error_t
seahorse_gpgme_key_op_sign_uid (SeahorseGpgmeUid    *uid,
                                SeahorseGpgmeKey    *signer,
                                SeahorseSignCheck    check,
                                SeahorseSignOptions  options)
{
    gpgme_key_t signing_key;
    gpgme_key_t signed_key;
    unsigned int sign_index;

    seahorse_gpgme_key_get_private (signer);

    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (uid), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (signer), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    signing_key = seahorse_gpgme_key_get_private (signer);
    g_return_val_if_fail (signing_key, GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    signed_key = seahorse_gpgme_uid_get_pubkey (uid);
    g_return_val_if_fail (signing_key, GPG_E (GPG_ERR_INV_VALUE));

    sign_index = seahorse_gpgme_uid_get_actual_index (uid);

    return sign_process (signed_key, signing_key, sign_index, check, options);
}

gpgme_error_t
seahorse_gpgme_key_op_sign (SeahorseGpgmeKey *pkey, SeahorseGpgmeKey *signer,
                            SeahorseSignCheck check, SeahorseSignOptions options)
{
    gpgme_key_t signing_key;
    gpgme_key_t signed_key;

    seahorse_gpgme_key_get_private (signer);

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (pkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (signer), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    signing_key = seahorse_gpgme_key_get_private (signer);
    g_return_val_if_fail (signing_key, GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    signed_key = seahorse_gpgme_key_get_public (pkey);

    return sign_process (signed_key, signing_key, 0, check, options);
}

static gboolean
on_key_op_change_pass_complete (gpgme_error_t gerr,
                                gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    g_autoptr(GError) error = NULL;

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return FALSE; /* don't call again */
    }

    seahorse_progress_end (g_task_get_cancellable (task), task);
    g_task_return_boolean (task, TRUE);
    return FALSE; /* don't call again */
}

/**
 * seahorse_gpgme_key_op_change_pass_async:
 * @pkey: The key that you want to change the password of
 * @cancellable: (nullable): A #GCancellable
 * @callback: The callback that will be called when the operation finishes
 * @user_data: (closure callback): User data passed on to @callback
 *
 * Changes the password of @pkey. The actual changing will be done by GPGME, so
 * this function doesn't allow to specify the new password.
 */
void
seahorse_gpgme_key_op_change_pass_async (SeahorseGpgmeKey *pkey,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         gpointer user_data)
{
    g_autoptr(GTask) task = NULL;
    gpgme_ctx_t gctx;
    gpgme_error_t gerr;
    g_autoptr(GError) error = NULL;
    g_autoptr(GSource) gsource = NULL;
    gpgme_key_t key;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (pkey));
    g_return_if_fail (seahorse_item_get_usage (SEAHORSE_ITEM (pkey)) == SEAHORSE_USAGE_PRIVATE_KEY);

    gctx = seahorse_gpgme_keyring_new_context (&gerr);

    task = g_task_new (pkey, cancellable, callback, user_data);
    gpgme_set_progress_cb (gctx, on_key_op_progress, task);
    g_task_set_task_data (task, gctx, (GDestroyNotify) gpgme_release);

    seahorse_progress_prep_and_begin (cancellable, task, NULL);
    gsource = seahorse_gpgme_gsource_new (gctx, cancellable);
    g_source_set_callback (gsource, (GSourceFunc)on_key_op_change_pass_complete,
                           g_object_ref (task), g_object_unref);

    key = seahorse_gpgme_key_get_private (pkey);
    if (gerr == 0)
        gerr = gpgme_op_passwd_start (gctx, key, 0);

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_source_attach (gsource, g_main_context_default ());
}

gboolean
seahorse_gpgme_key_op_change_pass_finish (SeahorseGpgmeKey *pkey,
                                          GAsyncResult *result,
                                          GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, pkey), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
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
edit_trust_action (unsigned int  state,
                   void         *data,
                   int           fd)
{
    int trust = GPOINTER_TO_INT (data);

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
static unsigned int
edit_trust_transit (unsigned int   current_state,
                    const char    *status,
                    const char    *args,
                    void          *data,
                    gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {
        /* start state */
        case TRUST_START:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = TRUST_COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
            }
            break;
        /* did command, next is value */
        case TRUST_COMMAND:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "edit_ownertrust.value"))
                next_state = TRUST_VALUE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
            }
            break;
        /* did value, go to quit or confirm ultimate */
        case TRUST_VALUE:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = TRUST_QUIT;
            else if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "edit_ownertrust.set_ultimate.okay"))
                next_state = TRUST_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
            }
            break;
        /* did confirm ultimate, go to quit */
        case TRUST_CONFIRM:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = TRUST_QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
            }
            break;
        /* did quit, go to confirm to finish op */
        case TRUST_QUIT:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
                next_state = TRUST_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TRUST_ERROR);
            }
            break;
        /* error, go to quit */
        case TRUST_ERROR:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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
 * seahorse_gpgme_key_op_set_trust:
 * @pkey: #SeahorseGpgmeKey whose trust will be changed
 * @trust: New trust value that must be at least #SEAHORSE_VALIDITY_NEVER.
 * If @pkey is a #SeahorseKeyPair, then @trust cannot be #SEAHORSE_VALIDITY_UNKNOWN.
 * If @pkey is not a #SeahorseKeyPair, then @trust cannot be #SEAHORSE_VALIDITY_ULTIMATE.
 *
 * Tries to change the owner trust of @pkey to @trust.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_gpgme_key_op_set_trust (SeahorseGpgmeKey *pkey, SeahorseValidity trust)
{
    SeahorseEditParm *parms;
    int menu_choice;

    g_debug ("[GPGME_KEY_OP] set_trust: trust = %i", trust);

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (pkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (trust >= SEAHORSE_VALIDITY_NEVER, GPG_E (GPG_ERR_INV_VALUE));
    g_return_val_if_fail (seahorse_gpgme_key_get_trust (pkey) != trust, GPG_E (GPG_ERR_INV_VALUE));

    if (seahorse_item_get_usage (SEAHORSE_ITEM (pkey)) == SEAHORSE_USAGE_PRIVATE_KEY)
        g_return_val_if_fail (trust != SEAHORSE_VALIDITY_UNKNOWN, GPG_E (GPG_ERR_INV_VALUE));
    else
        g_return_val_if_fail (trust != SEAHORSE_VALIDITY_ULTIMATE, GPG_E (GPG_ERR_INV_VALUE));

    switch (trust) {
        case SEAHORSE_VALIDITY_NEVER:
            menu_choice = GPG_NEVER;
            break;
        case SEAHORSE_VALIDITY_UNKNOWN:
            menu_choice = GPG_UNKNOWN;
            break;
        case SEAHORSE_VALIDITY_MARGINAL:
            menu_choice = GPG_MARGINAL;
            break;
        case SEAHORSE_VALIDITY_FULL:
            menu_choice = GPG_FULL;
            break;
        case SEAHORSE_VALIDITY_ULTIMATE:
            menu_choice = GPG_ULTIMATE;
            break;
        default:
            menu_choice = 1;
    }

    parms = seahorse_edit_parm_new (TRUST_START, edit_trust_action,
        edit_trust_transit, GINT_TO_POINTER (menu_choice));

    return edit_key (pkey, parms);
}

typedef enum {
    DISABLE_START,
    DISABLE_COMMAND,
    DISABLE_QUIT,
    DISABLE_ERROR
} DisableState;

/* action helper for disable/enable a key */
static gpgme_error_t
edit_disable_action (unsigned int state, gpointer data, int fd)
{
    const char *command = data;

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
static unsigned int
edit_disable_transit (unsigned int   current_state,
                      const char    *status,
                      const char    *args,
                      void          *data,
                      gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {
        /* start, do command */
        case DISABLE_START:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = DISABLE_COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DISABLE_ERROR);
            }
            break;
        /* did command, quit */
        case DISABLE_COMMAND:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = DISABLE_QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DISABLE_ERROR);
            }
        /* error, quit */
        case DISABLE_ERROR:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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
 * seahorse_gpgme_key_op_set_disabled:
 * @skey: #SeahorseKey to change
 * @disabled: New disabled state
 *
 * Tries to change disabled state of @skey to @disabled.
 *
 * Returns: Error value
 **/
gpgme_error_t
seahorse_gpgme_key_op_set_disabled (SeahorseGpgmeKey *pkey, gboolean disabled)
{
    char *command;
    SeahorseEditParm *parms;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (pkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    /* Get command and op */
    if (disabled)
        command = "disable";
    else
        command = "enable";

    parms = seahorse_edit_parm_new (DISABLE_START, edit_disable_action, edit_disable_transit, command);

    return edit_key (pkey, parms);
}

typedef struct
{
    unsigned int index;
    GDateTime *expires;
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
edit_expire_action (unsigned int state, gpointer data, int fd)
{
    ExpireParm *parm = (ExpireParm*)data;
    g_autofree char *expires_str = NULL;

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
            expires_str = parm->expires?
                g_date_time_format (parm->expires, "%Y-%m-%d") : g_strdup ("0");
            PRINT ((fd, expires_str));
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
static unsigned int
edit_expire_transit (unsigned int   current_state,
                     const char    *status,
                     const char    *args,
                     void          *data,
                     gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {
        /* start state, selected key */
        case EXPIRE_START:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = EXPIRE_SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
            }
            break;
        /* selected key, do command */
        case EXPIRE_SELECT:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = EXPIRE_COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
            }
            break;
        /* did command, set expires */
        case EXPIRE_COMMAND:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "keygen.valid"))
                next_state = EXPIRE_DATE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
            }
            break;
        /* set expires, quit */
        case EXPIRE_DATE:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = EXPIRE_QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
            }
            break;
        /* quit, save */
        case EXPIRE_QUIT:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
                next_state = EXPIRE_SAVE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (EXPIRE_ERROR);
            }
            break;
        /* error, quit */
        case EXPIRE_ERROR:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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

gpgme_error_t
seahorse_gpgme_key_op_set_expires (SeahorseGpgmeSubkey *subkey,
                                   GDateTime           *expires)
{
    GDateTime *old_expires;
    ExpireParm exp_parm;
    SeahorseEditParm *parms;
    SeahorsePgpKey *parent_key;
    gpgme_key_t key;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    old_expires = seahorse_pgp_subkey_get_expires (SEAHORSE_PGP_SUBKEY (subkey));
    g_return_val_if_fail (expires != old_expires, GPG_E (GPG_ERR_INV_VALUE));

    if (expires && old_expires)
        g_return_val_if_fail (!g_date_time_equal (old_expires, expires),
                              GPG_E (GPG_ERR_INV_VALUE));

    parent_key = seahorse_pgp_subkey_get_parent_key (SEAHORSE_PGP_SUBKEY (subkey));
    key = seahorse_gpgme_key_get_public (SEAHORSE_GPGME_KEY (parent_key));
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    exp_parm.index = seahorse_pgp_subkey_get_index (SEAHORSE_PGP_SUBKEY (subkey));
    exp_parm.expires = expires;

    parms = seahorse_edit_parm_new (EXPIRE_START, edit_expire_action, edit_expire_transit, &exp_parm);

    return edit_refresh_gpgme_key (NULL, key, parms);
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
add_revoker_action (unsigned int state, gpointer data, int fd)
{
    char *keyid = (char*)data;

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
static unsigned int
add_revoker_transit (unsigned int   current_state,
                     const char    *status,
                     const char    *args,
                     void          *data,
                     gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {
        /* start, do command */
        case ADD_REVOKER_START:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = ADD_REVOKER_COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
            }
            break;
        /* did command, select revoker */
        case ADD_REVOKER_COMMAND:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "keyedit.add_revoker"))
                next_state = ADD_REVOKER_SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
            }
            break;
        /* selected revoker, confirm */
        case ADD_REVOKER_SELECT:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "keyedit.add_revoker.okay"))
                next_state = ADD_REVOKER_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
            }
            break;
        /* confirmed, quit */
        case ADD_REVOKER_CONFIRM:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = ADD_REVOKER_QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
            }
            break;
        /* quit, confirm(=save) */
        case ADD_REVOKER_QUIT:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
                next_state = ADD_REVOKER_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ADD_REVOKER_ERROR);
            }
            break;
        /* error, quit */
        case ADD_REVOKER_ERROR:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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

gpgme_error_t
seahorse_gpgme_key_op_add_revoker (SeahorseGpgmeKey *pkey, SeahorseGpgmeKey *revoker)
{
    SeahorseEditParm *parms;
    const char *keyid;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (pkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (revoker), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (seahorse_item_get_usage (SEAHORSE_ITEM (pkey)) == SEAHORSE_USAGE_PRIVATE_KEY, GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (seahorse_item_get_usage (SEAHORSE_ITEM (revoker)) == SEAHORSE_USAGE_PRIVATE_KEY, GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    keyid = seahorse_pgp_key_get_keyid (SEAHORSE_PGP_KEY (pkey));
    g_return_val_if_fail (keyid, GPG_E (GPG_ERR_INV_VALUE));

    parms = seahorse_edit_parm_new (ADD_REVOKER_START, add_revoker_action,
                                    add_revoker_transit, (gpointer)keyid);

    return edit_key (pkey, parms);
}

static gboolean
on_key_op_add_uid_complete (gpgme_error_t gerr,
                            gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    g_autoptr(GError) error = NULL;

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return FALSE; /* don't call again */
    }

    seahorse_progress_end (g_task_get_cancellable (task), task);
    g_task_return_boolean (task, TRUE);
    return FALSE; /* don't call again */
}

void
seahorse_gpgme_key_op_add_uid_async (SeahorseGpgmeKey    *pkey,
                                     const char          *name,
                                     const char          *email,
                                     const char          *comment,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     void                *user_data)
{
    g_autoptr(GTask) task = NULL;
    gpgme_ctx_t gctx;
    gpgme_error_t gerr;
    g_autoptr(GError) error = NULL;
    g_autoptr(GSource) gsource = NULL;
    gpgme_key_t key;
    g_autofree char* uid = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (pkey));
    g_return_if_fail (seahorse_item_get_usage (SEAHORSE_ITEM (pkey)) == SEAHORSE_USAGE_PRIVATE_KEY);

    gctx = seahorse_gpgme_keyring_new_context (&gerr);

    task = g_task_new (pkey, cancellable, callback, user_data);
    gpgme_set_progress_cb (gctx, on_key_op_progress, task);
    g_task_set_task_data (task, gctx, (GDestroyNotify) gpgme_release);

    seahorse_progress_prep_and_begin (cancellable, task, NULL);
    gsource = seahorse_gpgme_gsource_new (gctx, cancellable);
    g_source_set_callback (gsource, (GSourceFunc)on_key_op_add_uid_complete,
                           g_object_ref (task), g_object_unref);

    key = seahorse_gpgme_key_get_private (pkey);
    uid = seahorse_pgp_uid_calc_label (name, email, comment);
    if (gerr == 0)
        gerr = gpgme_op_adduid_start (gctx, key, uid, 0);

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_source_attach (gsource, g_main_context_default ());
}

gboolean
seahorse_gpgme_key_op_add_uid_finish (SeahorseGpgmeKey *pkey,
                                      GAsyncResult *result,
                                      GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, pkey), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

void
seahorse_gpgme_key_op_add_subkey_async (SeahorseGpgmeKey        *pkey,
                                        SeahorsePgpKeyAlgorithm  algo,
                                        SeahorsePgpSubkeyUsage   usage,
                                        unsigned int             length,
                                        GDateTime               *expires,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        void                    *user_data)
{
    g_autoptr(GTask) task = NULL;
    gpgme_ctx_t gctx;
    gpgme_error_t gerr;
    g_autoptr(GError) error = NULL;
    gpgme_key_t key;
    const char *algo_str;
    g_autofree char *algo_full = NULL;
    int64_t expires_ts;
    g_autoptr(GSource) gsource = NULL;
    unsigned int flags = 0;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (pkey));
    g_return_if_fail (seahorse_pgp_key_is_private_key (SEAHORSE_PGP_KEY (pkey)));

    gctx = seahorse_gpgme_keyring_new_context (&gerr);

    task = g_task_new (pkey, cancellable, callback, user_data);
    gpgme_set_progress_cb (gctx, on_key_op_progress, task);
    g_task_set_task_data (task, gctx, (GDestroyNotify) gpgme_release);

    seahorse_progress_prep_and_begin (cancellable, task, NULL);
    gsource = seahorse_gpgme_gsource_new (gctx, cancellable);
    g_source_set_callback (gsource, (GSourceFunc)on_key_op_add_uid_complete,
                           g_object_ref (task), g_object_unref);

    /* Get the actual secret key */
    key = seahorse_gpgme_key_get_private (pkey);

    /* Get the algo string as GPG(ME) expects it */
    algo_str = seahorse_pgp_key_algorithm_to_gpgme_string (algo);
    g_return_if_fail (algo_str != NULL);
    algo_full = g_strdup_printf ("%s%u", algo_str, length);

    /* 0 means "no expire" for us (GPGME picks a default otherwise) */
    if (expires) {
        expires_ts = g_date_time_to_unix (expires);
    } else {
        expires_ts = 0;
        flags |= GPGME_CREATE_NOEXPIRE;
    }

    /* Add usage flags */
    switch (usage) {
        case SEAHORSE_PGP_SUBKEY_USAGE_SIGN_ONLY:
            flags |= GPGME_CREATE_SIGN;
            break;
        case SEAHORSE_PGP_SUBKEY_USAGE_ENCRYPT_ONLY:
            flags |= GPGME_CREATE_ENCR;
            break;
        default:
            break;
    }

    if (gerr == 0) {
        gerr = gpgme_op_createsubkey_start (gctx,
                                            key,
                                            algo_full,
                                            0,
                                            expires_ts,
                                            flags);
    }

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_source_attach (gsource, g_main_context_default ());
}

gboolean
seahorse_gpgme_key_op_add_subkey_finish (SeahorseGpgmeKey *pkey,
                                         GAsyncResult *result,
                                         GError **error)
{
    g_return_val_if_fail (g_task_is_valid (result, pkey), FALSE);

    seahorse_gpgme_key_refresh (pkey);
    return g_task_propagate_boolean (G_TASK (result), error);
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
del_key_action (unsigned int state, gpointer data, int fd)
{
    switch (state) {
        /* select key */
        case DEL_KEY_SELECT:
            PRINTF ((fd, "key %d", GPOINTER_TO_UINT (data)));
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
static unsigned int
del_key_transit (unsigned int   current_state,
                 const char    *status,
                 const char    *args,
                 void          *data,
                 gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {
        /* start, select key */
        case DEL_KEY_START:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = DEL_KEY_SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DEL_KEY_ERROR);
            }
            break;
        /* selected key, do command */
        case DEL_KEY_SELECT:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = DEL_KEY_COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DEL_KEY_ERROR);
            }
            break;
        case DEL_KEY_COMMAND:
            /* did command, confirm */
            if (g_str_equal (status, "GET_BOOL") && g_str_equal
            (args, "keyedit.remove.subkey.okay"))
                next_state = DEL_KEY_CONFIRM;
            /* did command, quit */
            else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
                next_state = DEL_KEY_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DEL_KEY_ERROR);
            }
            break;
        /* error, quit */
        case DEL_KEY_ERROR:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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

gpgme_error_t
seahorse_gpgme_key_op_del_subkey (SeahorseGpgmeSubkey *subkey)
{
    SeahorsePgpKey *parent_key;
    gpgme_key_t key;
    SeahorseEditParm *parms;
    int index;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    parent_key = seahorse_pgp_subkey_get_parent_key (SEAHORSE_PGP_SUBKEY (subkey));
    key = seahorse_gpgme_key_get_public (SEAHORSE_GPGME_KEY (parent_key));
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    index = seahorse_pgp_subkey_get_index (SEAHORSE_PGP_SUBKEY (subkey));
    parms = seahorse_edit_parm_new (DEL_KEY_START, del_key_action,
                                    del_key_transit, GUINT_TO_POINTER (index));

    return edit_refresh_gpgme_key (NULL, key, parms);
}

typedef struct
{
    unsigned int             index;
    SeahorsePgpRevokeReason  reason;
    const char              *description;
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
rev_subkey_action (unsigned int state, gpointer data, int fd)
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
            PRINTF ((fd, "%s", parm->description));
            break;
        case REV_SUBKEY_ENDDESC:
            /* Need empty line, which is written at the end */
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
static unsigned int
rev_subkey_transit (unsigned int   current_state,
                    const char    *status,
                    const char    *args,
                    void          *data,
                    gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {
        /* start, select key */
        case REV_SUBKEY_START:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = REV_SUBKEY_SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        /* selected key, do command */
        case REV_SUBKEY_SELECT:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = REV_SUBKEY_COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        /* did command, confirm */
        case REV_SUBKEY_COMMAND:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "keyedit.revoke.subkey.okay"))
                next_state = REV_SUBKEY_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        case REV_SUBKEY_CONFIRM:
            /* did confirm, do reason */
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "ask_revocation_reason.code"))
                next_state = REV_SUBKEY_REASON;
            /* did confirm, quit */
            else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
                next_state = REV_SUBKEY_QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        /* did reason, do description */
        case REV_SUBKEY_REASON:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "ask_revocation_reason.text"))
                next_state = REV_SUBKEY_DESCRIPTION;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        case REV_SUBKEY_DESCRIPTION:
            /* did description, end it */
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "ask_revocation_reason.text"))
                next_state = REV_SUBKEY_ENDDESC;
            /* did description, confirm */
            else if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "ask_revocation_reason.okay"))
                next_state = REV_SUBKEY_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        /* ended description, confirm */
        case REV_SUBKEY_ENDDESC:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "ask_revocation_reason.okay"))
                next_state = REV_SUBKEY_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        /* quit, confirm(=save) */
        case REV_SUBKEY_QUIT:
            if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
                next_state = REV_SUBKEY_CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (REV_SUBKEY_ERROR);
            }
            break;
        /* error, quit */
        case REV_SUBKEY_ERROR:
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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

gpgme_error_t
seahorse_gpgme_key_op_revoke_subkey (SeahorseGpgmeSubkey    *subkey,
                                     SeahorsePgpRevokeReason reason,
                                     const char             *description)
{
    RevSubkeyParm rev_parm;
    SeahorseEditParm *parms;
    gpgme_subkey_t gsubkey;
    SeahorsePgpKey *parent_key;
    gpgme_key_t key;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    gsubkey = seahorse_gpgme_subkey_get_subkey (subkey);
    g_return_val_if_fail (!gsubkey->revoked, GPG_E (GPG_ERR_INV_VALUE));

    parent_key = seahorse_pgp_subkey_get_parent_key (SEAHORSE_PGP_SUBKEY (subkey));
    key = seahorse_gpgme_key_get_public (SEAHORSE_GPGME_KEY (parent_key));
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    rev_parm.index = seahorse_pgp_subkey_get_index (SEAHORSE_PGP_SUBKEY (subkey));
    rev_parm.reason = reason;
    rev_parm.description = description;

    parms = seahorse_edit_parm_new (REV_SUBKEY_START, rev_subkey_action,
                                    rev_subkey_transit, &rev_parm);

    return edit_refresh_gpgme_key (NULL, key, parms);
}

typedef struct {
    unsigned int index;
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
primary_action (unsigned int state, gpointer data, int fd)
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
static unsigned int
primary_transit (unsigned int   current_state,
                 const char    *status,
                 const char    *args,
                 void          *data,
                 gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {

    /* start, select key */
    case PRIMARY_START:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = PRIMARY_SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;

    /* selected key, do command */
    case PRIMARY_SELECT:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = PRIMARY_COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;

    /* did command, quit */
    case PRIMARY_COMMAND:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = PRIMARY_QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;

    /* quitting so save */
    case PRIMARY_QUIT:
        if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
            next_state = PRIMARY_SAVE;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PRIMARY_ERROR);
        }
        break;

    /* error, quit */
    case PRIMARY_ERROR:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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

static gboolean
on_key_op_make_primary_complete (gpgme_error_t gerr,
                                 gpointer user_data)
{
    GTask *task = G_TASK (user_data);
    SeahorseGpgmeUid *uid = SEAHORSE_GPGME_UID (g_task_get_source_object (task));
    SeahorsePgpKey *parent_key = NULL;
    g_autoptr(GError) error = NULL;

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return FALSE; /* don't call again */
    }

    parent_key = seahorse_pgp_uid_get_parent (SEAHORSE_PGP_UID (uid));
    seahorse_gpgme_key_refresh (SEAHORSE_GPGME_KEY (parent_key));

    g_task_return_boolean (task, TRUE);
    return FALSE; /* don't call again */
}

void
seahorse_gpgme_key_op_make_primary_async (SeahorseGpgmeUid *uid,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
    g_autoptr(GTask) task = NULL;
    gpgme_ctx_t gctx;
    gpgme_error_t gerr;
    g_autoptr(GError) error = NULL;
    g_autoptr(GSource) gsource = NULL;
    gpgme_key_t key = NULL;
    gpgme_user_id_t gpg_uid = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_UID (uid));

    gpg_uid = seahorse_gpgme_uid_get_userid (uid);
    g_return_if_fail (!gpg_uid->revoked && !gpg_uid->invalid);

    key = seahorse_gpgme_uid_get_pubkey (uid);
    g_return_if_fail (key);

    gctx = seahorse_gpgme_keyring_new_context (&gerr);

    task = g_task_new (uid, cancellable, callback, user_data);
    gpgme_set_progress_cb (gctx, on_key_op_progress, task);
    g_task_set_task_data (task, gctx, (GDestroyNotify) gpgme_release);

    seahorse_progress_prep_and_begin (cancellable, task, NULL);
    gsource = seahorse_gpgme_gsource_new (gctx, cancellable);
    g_source_set_callback (gsource, G_SOURCE_FUNC (on_key_op_make_primary_complete),
                           g_object_ref (task), g_object_unref);

    if (gerr == 0)
        gerr = gpgme_op_set_uid_flag_start (gctx, key, gpg_uid->uid, "primary", NULL);

    if (seahorse_gpgme_propagate_error (gerr, &error)) {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

    g_source_attach (gsource, g_main_context_default ());
}

gboolean
seahorse_gpgme_key_op_make_primary_finish (SeahorseGpgmeUid *uid,
                                           GAsyncResult *result,
                                           GError **error)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (uid), FALSE);
    g_return_val_if_fail (g_task_is_valid (result, uid), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}


typedef struct {
    unsigned int index;
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
del_uid_action (unsigned int state, gpointer data, int fd)
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
static unsigned int
del_uid_transit (unsigned int   current_state,
                 const char    *status,
                 const char    *args,
                 void          *data,
                 gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {

    /* start, select key */
    case DEL_UID_START:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = DEL_UID_SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DEL_UID_ERROR);
        }
        break;

    /* selected key, do command */
    case DEL_UID_SELECT:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = DEL_UID_COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DEL_UID_ERROR);
        }
        break;

    /* did command, confirm */
    case DEL_UID_COMMAND:
        if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "keyedit.remove.uid.okay"))
            next_state = DEL_UID_CONFIRM;
        else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = DEL_UID_QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (REV_SUBKEY_ERROR);
        }
        break;

    /* confirmed, quit */
    case DEL_UID_CONFIRM:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = DEL_UID_QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DEL_UID_ERROR);
        }
        break;

    /* quitted so save */
    case DEL_UID_QUIT:
        if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE))
            next_state = DEL_UID_SAVE;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (REV_SUBKEY_ERROR);
        }
        break;

    /* error, quit */
    case DEL_UID_ERROR:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
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
seahorse_gpgme_key_op_del_uid (SeahorseGpgmeUid *uid)
{
    DelUidParm del_uid_parm;
    SeahorseEditParm *parms;
    gpgme_key_t key;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_UID (uid), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    key = seahorse_gpgme_uid_get_pubkey (uid);
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    del_uid_parm.index = seahorse_gpgme_uid_get_actual_index (uid);

    parms = seahorse_edit_parm_new (DEL_UID_START, del_uid_action,
                                    del_uid_transit, &del_uid_parm);

    return edit_refresh_gpgme_key (NULL, key, parms);
}

typedef struct {
    const char *filename;
} PhotoIdAddParm;

typedef enum {
    PHOTO_ID_ADD_START,
    PHOTO_ID_ADD_COMMAND,
    PHOTO_ID_ADD_URI,
    PHOTO_ID_ADD_BIG,
    PHOTO_ID_ADD_QUIT,
    PHOTO_ID_ADD_SAVE,
    PHOTO_ID_ADD_ERROR
} PhotoIdAddState;

/* action helper for adding a photoid to a #SeahorseKey */
static gpgme_error_t
photoid_add_action (unsigned int state, gpointer data, int fd)
{
    PhotoIdAddParm *parm = (PhotoIdAddParm*)data;

    switch (state) {
    case PHOTO_ID_ADD_COMMAND:
        PRINT ((fd, "addphoto"));
        break;
    case PHOTO_ID_ADD_URI:
        PRINT ((fd, parm->filename));
        break;
    case PHOTO_ID_ADD_BIG:
        PRINT ((fd, YES));
        break;
    case PHOTO_ID_ADD_QUIT:
        PRINT ((fd, QUIT));
        break;
    case PHOTO_ID_ADD_SAVE:
        PRINT ((fd, YES));
        break;
    default:
        g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
        break;
    }

    seahorse_util_print_fd (fd, "\n");
    return GPG_OK;
}

static unsigned int
photoid_add_transit (unsigned int   current_state,
                     const char    *status,
                     const char    *args,
                     void          *data,
                     gpgme_error_t *err)
{
    unsigned int next_state;

    switch (current_state) {

    case PHOTO_ID_ADD_START:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = PHOTO_ID_ADD_COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_ADD_ERROR);
        }
        break;
    case PHOTO_ID_ADD_COMMAND:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "photoid.jpeg.add")) {
            next_state = PHOTO_ID_ADD_URI;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_ADD_ERROR);
        }
        break;
   case PHOTO_ID_ADD_URI:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT)) {
            next_state = PHOTO_ID_ADD_QUIT;
        } else if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, "photoid.jpeg.size")) {
            next_state = PHOTO_ID_ADD_BIG;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_ADD_ERROR);
        }
        break;
    case PHOTO_ID_ADD_BIG:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT)) {
            next_state = PHOTO_ID_ADD_QUIT;
        /* This happens when the file is invalid or can't be accessed */
        } else if (g_str_equal (status, "GET_LINE") && g_str_equal (args, "photoid.jpeg.add")) {
            *err = GPG_E (GPG_ERR_USER_1);
            return PHOTO_ID_ADD_ERROR;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_ADD_ERROR);
        }
        break;
    case PHOTO_ID_ADD_QUIT:
        if (g_str_equal (status, "GET_BOOL") && g_str_equal (args, SAVE)) {
            next_state = PHOTO_ID_ADD_SAVE;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_ADD_ERROR);
        }
        break;
    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (PHOTO_ID_ADD_ERROR);
        break;
    }

    return next_state;
}

gpgme_error_t
seahorse_gpgme_key_op_photo_add (SeahorseGpgmeKey *pkey, const char *filename)
{
    SeahorseEditParm *parms;
    PhotoIdAddParm photoid_add_parm;
    gpgme_key_t key;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (pkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));
    g_return_val_if_fail (filename, GPG_E (GPG_ERR_INV_VALUE));

    key = seahorse_gpgme_key_get_public (pkey);
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    photoid_add_parm.filename = filename;

    parms = seahorse_edit_parm_new (PHOTO_ID_ADD_START, photoid_add_action,
                                    photoid_add_transit, &photoid_add_parm);

    return edit_refresh_gpgme_key (NULL, key, parms);
}

gpgme_error_t
seahorse_gpgme_key_op_photo_delete (SeahorseGpgmePhoto *photo)
{
    DelUidParm del_uid_parm;
    SeahorseEditParm *parms;
    gpgme_key_t key;

    g_return_val_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    key = seahorse_gpgme_photo_get_pubkey (photo);
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    del_uid_parm.index = seahorse_gpgme_photo_get_index (photo);

    parms = seahorse_edit_parm_new (DEL_UID_START, del_uid_action,
                                    del_uid_transit, &del_uid_parm);

    return edit_refresh_gpgme_key (NULL, key, parms);
}

typedef struct {
    GList *photos;
    unsigned int uid;
    unsigned int num_uids;
    char *output_file;
    gpgme_key_t key;
} PhotoIdLoadParm;

typedef enum {
    PHOTO_ID_LOAD_START,
    PHOTO_ID_LOAD_SELECT,
    PHOTO_ID_LOAD_OUTPUT_IMAGE,
    PHOTO_ID_LOAD_DESELECT,
    PHOTO_ID_LOAD_QUIT,
    PHOTO_ID_LOAD_ERROR
} PhotoIdLoadState;

/* action helper for getting a list of photoids attached to a #SeahorseKey */
static gpgme_error_t
photoid_load_action (unsigned int state, gpointer data, int fd)
{
    PhotoIdLoadParm *parm = (PhotoIdLoadParm*)data;

    switch (state) {
        case PHOTO_ID_LOAD_SELECT:
            PRINTF ((fd, "uid %d", parm->uid));
            break;
        case PHOTO_ID_LOAD_OUTPUT_IMAGE:
            PRINT ((fd, "showphoto"));
            break;
        case PHOTO_ID_LOAD_DESELECT:
            PRINTF ((fd, "uid %d", parm->uid));
            break;
        case PHOTO_ID_LOAD_QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
            break;
    }

    seahorse_util_print_fd (fd, "\n");
    return GPG_OK;
}

static unsigned int
photoid_load_transit (unsigned int   current_state,
                      const char    *status,
                      const char    *args,
                      void          *data,
                      gpgme_error_t *err)
{
    PhotoIdLoadParm *parm = (PhotoIdLoadParm*)data;
    SeahorseGpgmePhoto *photo;
    GdkPixbuf *pixbuf = NULL;
    unsigned int next_state = 0;
    struct stat st;
    GError *error = NULL;

    switch (current_state) {

    /* start, get photoid list */
    case PHOTO_ID_LOAD_START:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT))
            next_state = PHOTO_ID_LOAD_SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_LOAD_ERROR);
        }
        break;

    case PHOTO_ID_LOAD_SELECT:
        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT)) {
            next_state = PHOTO_ID_LOAD_OUTPUT_IMAGE;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_LOAD_ERROR);
        }
        break;

    case PHOTO_ID_LOAD_OUTPUT_IMAGE:

        if (g_file_test (parm->output_file, G_FILE_TEST_EXISTS)) {

            if (g_stat (parm->output_file, &st) == -1) {
                g_warning ("couldn't stat output image file '%s': %s", parm->output_file,
                           g_strerror (errno));

            } else if (st.st_size > 0) {
                pixbuf = gdk_pixbuf_new_from_file (parm->output_file, &error);
                if (pixbuf == NULL) {
                    g_warning ("Loading image %s failed: %s", parm->output_file,
                               error && error->message ? error->message : "unknown");
                    g_error_free (error);
                }
            }

            g_unlink (parm->output_file);

            photo = seahorse_gpgme_photo_new (parm->key, pixbuf, parm->uid);
            parm->photos = g_list_append (parm->photos, photo);

            g_object_unref (pixbuf);
        }

        if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT)) {
            next_state = PHOTO_ID_LOAD_DESELECT;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PHOTO_ID_LOAD_ERROR);
        }
        break;

    case PHOTO_ID_LOAD_DESELECT:
        if (parm->uid < parm->num_uids) {
            parm->uid = parm->uid + 1;
            g_debug ("PhotoIDLoad Next UID %i", parm->uid);
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT)) {
                next_state = PHOTO_ID_LOAD_SELECT;
            } else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PHOTO_ID_LOAD_ERROR);
            }
        } else {
            if (g_str_equal (status, "GET_LINE") && g_str_equal (args, PROMPT)) {
                next_state = PHOTO_ID_LOAD_QUIT;
                g_debug ("PhotoIDLoad Quiting Load");
            } else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PHOTO_ID_LOAD_ERROR);
            }
        }
        break;

    case PHOTO_ID_LOAD_QUIT:
        /* Shouldn't be reached */
        *err = GPG_E (GPG_ERR_GENERAL);
        g_debug ("PhotoIDLoad Reached Quit");
        g_return_val_if_reached (PHOTO_ID_LOAD_ERROR);
        break;

    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (PHOTO_ID_LOAD_ERROR);
        break;
    }

    return next_state;
}

gpgme_error_t
seahorse_gpgme_key_op_photos_load (SeahorseGpgmeKey *pkey)
{
    /* Make sure there's enough room for the .jpg extension */
    char image_path[]    = "/tmp/seahorse-photoid-XXXXXX\0\0\0\0";

    PhotoIdLoadParm photoid_load_parm;
    gpgme_error_t gerr;
    gpgme_key_t key;
    const char *keyid;
    int fd;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (pkey), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    key = seahorse_gpgme_key_get_public (pkey);
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));
    g_return_val_if_fail (key->subkeys && key->subkeys->keyid, GPG_E (GPG_ERR_INV_VALUE));
    keyid = key->subkeys->keyid;

    g_debug ("PhotoIDLoad Start");

    fd = g_mkstemp (image_path);
    if(fd == -1) {
        gerr = GPG_E(GPG_ERR_GENERAL);

    } else {

        g_unlink(image_path);
        close(fd);
        strcat (image_path, ".jpg");

        photoid_load_parm.uid = 1;
        photoid_load_parm.num_uids = 0;
        photoid_load_parm.photos = NULL;
        photoid_load_parm.output_file = image_path;
        photoid_load_parm.key = key;

        g_debug ("PhotoIdLoad KeyID %s", keyid);
        gerr = seahorse_gpg_op_num_uids (NULL, keyid, &(photoid_load_parm.num_uids));
        g_debug ("PhotoIDLoad Number of UIDs %i", photoid_load_parm.num_uids);

        if (GPG_IS_OK (gerr)) {
            const char *oldpath;
            g_autofree char *path = NULL;
            SeahorseEditParm *parms;

            setenv ("SEAHORSE_IMAGE_FILE", image_path, 1);
            oldpath = getenv("PATH");

            path = g_strdup_printf ("%s:%s", EXECDIR, oldpath);
            setenv ("PATH", path, 1);

            parms = seahorse_edit_parm_new (PHOTO_ID_LOAD_START, photoid_load_action,
                                            photoid_load_transit, &photoid_load_parm);

            /* generate list */
            gerr = edit_gpgme_key (NULL, key, parms);
            setenv ("PATH", oldpath, 1);

            if (GPG_IS_OK (gerr)) {
                for (GList *p = photoid_load_parm.photos; p; p = p->next) {
                    seahorse_pgp_key_add_photo (SEAHORSE_PGP_KEY (pkey), p->data);
                }
            }
        }

        g_list_free_full (photoid_load_parm.photos, g_object_unref);
    }

    g_debug ("PhotoIDLoad Done");

    return gerr;
}

gpgme_error_t
seahorse_gpgme_key_op_photo_primary (SeahorseGpgmePhoto *photo)
{
    PrimaryParm pri_parm;
    SeahorseEditParm *parms;
    gpgme_key_t key;

    g_return_val_if_fail (SEAHORSE_IS_GPGME_PHOTO (photo), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    key = seahorse_gpgme_photo_get_pubkey (photo);
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    pri_parm.index = seahorse_gpgme_photo_get_index (photo);

    parms = seahorse_edit_parm_new (PRIMARY_START, primary_action,
                                    primary_transit, &pri_parm);

    return edit_refresh_gpgme_key (NULL, key, parms);
}
