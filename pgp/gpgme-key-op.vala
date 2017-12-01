/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004 Stefan Walter
 * Copyright (C) 2005 Adam Schreiber
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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

namespace Seahorse.GpgME {

/*
 * Key type options.
 * We only support GPG version >=2.0.12 or >= 2.2.0
 */
public enum KeyEncType {
    RSA_RSA = 1,
    DSA_ELGAMAL = 2,
    DSA = 3,
    RSA_SIGN = 4,
    ELGAMAL = 5,
    RSA_ENCRYPT = 6
}

/* Length ranges for key types */
public enum SeahorseKeyLength {
    /* Minimum length for #DSA. */
    DSA_MIN = 768,
    /* Maximum length for #DSA. */
#if ( GPG_MAJOR == 2 &&   GPG_MINOR == 0 && GPG_MICRO < 12 ) || \
    ( GPG_MAJOR == 1 && ( GPG_MINOR <  4 || GPG_MICRO < 10 ) )
    DSA_MAX = 1024,
#else
    DSA_MAX = 3072,
#endif
    /* Minimum length for #ELGAMAL. Maximum length is #LENGTH_MAX. */
    ELGAMAL_MIN = 768,
    /* Minimum length of #RSA_SIGN and #RSA_ENCRYPT. Maximum length is
     * #LENGTH_MAX.
     */
    RSA_MIN = 1024,
    /* Maximum length for #ELGAMAL, #RSA_SIGN, and #RSA_ENCRYPT. */
    LENGTH_MAX = 4096,
    /* Default length for #ELGAMAL, #RSA_SIGN, #RSA_ENCRYPT, and #DSA. */
    LENGTH_DEFAULT = 2048
}

public enum SignCheck {
    /* Unknown key check */
    SIGN_CHECK_NO_ANSWER,
    /* Key not checked */
    SIGN_CHECK_NONE,
    /* Key casually checked */
    SIGN_CHECK_CASUAL,
    /* Key carefully checked */
    SIGN_CHECK_CAREFUL
}

[Flags]
public enum SignOptions {
    /* If signature is local */
    SIGN_LOCAL,
    /* If signature is non-revocable */
    SIGN_NO_REVOKE,
    /* If signature expires with key */
    SIGN_EXPIRES;
}

// namespace or class?
namespace KeyOperation {

public const string PROMPT = "keyedit.prompt";
public const string QUIT = "quit";
public const string SAVE = "keyedit.save.okay";
public const string YES = "Y";
public const string NO = "N";

// XXX no macro's in vala
// #define PRINT(args)  if(!Seahorse.Util.print_fd args) return GPG_E (GPG_ERR_GENERAL)
// #define PRINTF(args) if(!Seahorse.Util.printf_fd args) return GPG_E (GPG_ERR_GENERAL)

// XXX this isn't used I think?
// #define GPG_UNKNOWN     1
// #define GPG_NEVER       2
// #define GPG_MARGINAL    3
// #define GPG_FULL        4
// #define GPG_ULTIMATE    5

/**
 * Tries to generate a new key based on given parameters.
 *
 * @param sksrc #SeahorseSource
 * @param name User ID name, must be at least 5 characters long
 * @param email Optional user ID email
 * @param comment Optional user ID comment
 * @param passphrase Passphrase for key
 * @param type Key type. Supported types are #DSA_ELGAMAL, #DSA, #RSA_SIGN, and #RSA_RSA
 * @param length Length of key, must be within the range of @type specified by #SeahorseKeyLength
 * @param expires Expiration date of key
 **/
public async void generate_async(Keyring keyring, string name, string? email, string? comment,
                                 string passphrase, SeahorseKeyEncType type, uint length,
                                 time_t expires, Cancellable cancellable) throws GLib.Error {
    if (name == null || name.length < 5 || passphrase == null)
        return;

    if (!type.bits_supported(length))
        return;

    string email_part = "", comment_part = "";
    if (email != null && email != "")
        email_part = "Name-Email: %s\n".printf(email);
    if (comment != null && comment != "")
        comment_part = "Name-Comment: %s\n".printf(comment);

    string expires_date = (expires != 0)? Seahorse.Util.get_date_string(expires) : "0";

    // Subkey xml
    string subkey_part = "";
    if (type == DSA_ELGAMAL)
        subkey_part = "Subkey-Type: ELG-E\nSubkey-Length: %d\nSubkey-Usage: encrypt\n".printf(length);
    else if (type == RSA_RSA)
        subkey_part = "Subkey-Type: RSA\nSubkey-Length: %d\nSubkey-Usage: encrypt\n".printf(length);

    // Common xml XXX do this with vala interpolated strings?
    StringBuilder parms = new StringBuilder();
    parms.append("<GnupgKeyParms format=\"internal\">\n")
             .append_printf("Key-Type: %s\n", (type == DSA || type == DSA_ELGAMAL)? "DSA" : "RSA")
             .append_printf("Key-Length: %d\n", length) // shouldn't this be %u?
             .append(subkey_part)
             .append_printf("Name-Real: %s\n", name)
             .append(email_part)
             .append(comment_part)
             .append_printf("Expire-Date: %s\n", expires_date)
             .append_printf("Passphrase: %s\n", passphrase)
         .append("</GnupgKeyParms>");

    GPGError.ErrorCode gerr = GPGError.ErrorCode.NO_ERROR;
    GPG.Context gctx = Keyring.new_context(out gerr);
    gpgme_set_progress_cb(gctx, on_generate_progress, res);

    Seahorse.Progress.prep_and_begin(cancellable, res, null);
    GLib.Source gsource = seahorse_gpgme_gsource_new (gctx, cancellable);
    gsource.set_callback(on_key_op_generate_complete, res, g_object_unref);

    if (gerr == GPGError.ErrorCode.NO_ERROR)
        gerr = gpgme_op_genkey_start (gctx, parms.str, null, null); // XXX this is deprecated

    GLib.Error error = null;
    if (seahorse_gpgme_propagate_error (gerr, out error)) {
        g_simple_async_result_take_error (res, error);
        g_simple_async_result_complete_in_idle (res);
    } else {
        gsource.attach(g_main_context_default ());
    }
    g_source_unref (gsource);
}

private void on_generate_progress(void* opaque, string what, int type, int current, int total) {
    GSimpleAsyncResult *res = G_SIMPLE_ASYNC_RESULT (opaque);
    key_op_generate_closure *closure = g_simple_async_result_get_op_res_gpointer (res);
    Seahorse.Progress.update(closure.cancellable, res, "%s", what);
}

private bool on_key_op_generate_complete (GPGError.ErrorCode gerr) {
    GError *error = null;
    if (seahorse_gpgme_propagate_error (gerr, &error))
        g_simple_async_result_take_error (res, error);

    Seahorse.Progress.end(closure.cancellable, res);
    g_simple_async_result_complete (res);
    return false; /* don't call again */
}

public GPGError.ErrorCode delete(Key pkey) {
    return op_delete(pkey, false);
}

public GPGError.ErrorCode delete_pair(Key pkey) {
    return op_delete(pkey, true);
}

/* helper function for deleting @skey */
private GPGError.ErrorCode op_delete(Key pkey, bool secret) {
    Keyring keyring = (Keyring) pkey.place;
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), GPG_E (GPG_ERR_INV_KEYRING));

    GPG.Key? key = pkey.get_public();
    Seahorse.Util.wait_until(key != null);

    GPGError.ErrorCode gerr;
    GPG.Context ctx = Keyring.new_context(out gerr);
    if (ctx == null)
        return gerr;

    gerr = gpgme_op_delete (ctx, key, secret);
    if (gerr.is_success())
        keyring.remove_key(SEAHORSE_GPGME_KEY (pkey));

    return gerr;
}

/* Main key edit setup, structure, and a good deal of method content borrowed from gpa */

/* Edit action function */
// XXX typedef GPGError.ErrorCode (*EditAction) (uint state, void* data, int fd);
/* Edit transit function */
// XXX typedef uint (*EditTransit) (uint current_state, GPG.StatusCode status, string args, void* data, GPGError.ErrorCode *err);

/* Edit parameters */
private struct EditParm {
    uint state;
    GPGError.ErrorCode err;
    EditAction action;
    EditTransit transit;
    gpointer data;

    /* Creates new edit parameters with defaults */
    public EditParm(uint state, EditAction action, EditTransit transit, gpointer data) {
        this.state = state;
        this.err = GPG_OK;
        this.action = action;
        this.transit = transit;
        this.data = data;
    }
}

/* Edit callback for gpgme */
private GPGError.ErrorCode seahorse_gpgme_key_op_edit(EditParm data, GPG.StatusCode status, string args, int fd) {
    // Ignore these status lines, as they don't require any response
    if (status == GPG.StatusCode.EOF || status == GPG.StatusCode.GOT_IT ||
        status == GPG.StatusCode.NEED_PASSPHRASE || status == GPG.StatusCode.GOOD_PASSPHRASE ||
        status == GPG.StatusCode.BAD_PASSPHRASE || status == GPG.StatusCode.USERID_HINT ||
        status == GPG.StatusCode.SIGEXPIRED || status == GPG.StatusCode.KEYEXPIRED ||
        status == GPG.StatusCode.PROGRESS || status == GPG.StatusCode.KEY_CREATED ||
        status == GPG.StatusCode.ALREADY_SIGNED || status == GPG.StatusCode.MISSING_PASSPHRASE ||
        status == GPG.StatusCode.KEY_CONSIDERED)
        return parms.err;

    debug("[edit key] state: %d / status: %d / args: %s", parms.state, status, args);

    /* Choose the next state based on the current one and the input */
    parms.state = parms.transit(parms.state, status, args, parms.data, &parms.err);

    /* Choose the action based on the state */
    if (parms.err.is_success())
        parms.err = parms.action (parms.state, parms.data, fd);

    return parms.err;
}

/* Common edit operation */
// NOTE NOTE NOTE NOTE NOTE
private GPGError.ErrorCode edit_gpgme_key(GPG.Context? ctx, GPG.Key key, EditParm parms) {
    assert(key != null);
    assert(parms != null);

    GPGError.ErrorCode gerr;
    bool own_context = false;
    if (ctx == null) {
        ctx = Keyring.new_context(out gerr);
        if (ctx == null)
            return gerr;
        own_context = true;
    }

    GPG.Data data_out = seahorse_gpgme_data_new ();

    /* do edit callback, release data */
    gerr = gpgme_op_edit (ctx, key, seahorse_gpgme_key_op_edit, parms, data_out);

    if (gpgme_err_code (gerr) == GPG_ERR_BAD_PASSPHRASE) {
        Seahorse.Util.show_error(null, _("Wrong password"), _("This was the third time you entered a wrong password. Please try again."));
    }

    seahorse_gpgme_data_release (data_out);
    if (own_context)
        gpgme_release (ctx);

    return gerr;
}

private GPGError.ErrorCode edit_key(Key pkey, EditParm parms) {
    Keyring keyring = (Keyring) pkey.place;
    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), GPG_E (GPG_ERR_INV_KEYRING));

    GPG.Key key = pkey.get_public();
    Seahorse.Util.wait_until(key != null);

    GPGError.ErrorCode gerr;
    GPG.Context ctx = Keyring.new_context(out gerr);
    if (ctx != null) {
        gerr = edit_refresh_gpgme_key (ctx, key, parms);
    }

    return gerr;
}

// NOTE NOTE NOTE NOTE NOTE NOTE
private GPGError.ErrorCode edit_refresh_gpgme_key (GPG.Context ctx, GPG.Key key, EditParm *parms) {
    GPGError.ErrorCode gerr = edit_gpgme_key (ctx, key, parms);
    if (gerr.is_success())
        key.refresh_matching();

    return gerr;
}

private struct SignParm {
    uint index;
    string command;
    bool expire;
    SignCheck check;
}

private enum SignState {
    START,
    UID,
    COMMAND,
    EXPIRE,
    CONFIRM,
    CHECK,
    QUIT,
    ERROR,
}

/* action helper for signing a key */
private GPGError.ErrorCode sign_action(SignState state, SignParm parm, int fd) {
    switch (state) {
        /* select uid */
        case SignState.UID:
            PRINTF ((fd, "uid %d", parm.index));
            break;
        case SignState.COMMAND:
            PRINT ((fd, parm.command));
            break;
        /* if expires */
        case SignState.EXPIRE:
            PRINT ((fd, (parm.expire) ? YES : "N"));
            break;
        case SignState.CONFIRM:
            PRINT ((fd, YES));
            break;
        case SignState.CHECK:
            PRINTF ((fd, "%d", parm.check));
            break;
        case SignState.QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for signing a key */
private uint sign_transit(SignState current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start state, need to select uid */
        case SignState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = SignState.UID;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SignState.ERROR);
            }
            break;
        /* selected uid, go to command */
        case SignState.UID:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = SignState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SignState.ERROR);
            }
            break;
        case SignState.COMMAND:
            /* if doing all uids, confirm */
            if (status == GPG.StatusCode.GET_BOOL && (args == "keyedit.sign_all.okay"))
                next_state = SignState.CONFIRM;
            else if (status == GPG.StatusCode.GET_BOOL && (args == "sign_uid.okay"))
                next_state = SignState.CONFIRM;
            /* need to do expires */
            else if (status == GPG.StatusCode.GET_LINE && (args == "sign_uid.expire"))
                next_state = SignState.EXPIRE;
            /*  need to do check */
            else if (status == GPG.StatusCode.GET_LINE && (args == "sign_uid.class"))
                next_state = SignState.CHECK;
            /* if it's already signed then send back an error */
            else if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
                next_state = SignState.ERROR;
                *err = GPG_E (GPG_ERR_EALREADY);
            /* All other stuff is unexpected */
            } else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SignState.ERROR);
            }
            break;
        /* did expire, go to check */
        case SignState.EXPIRE:
            if (status == GPG.StatusCode.GET_LINE && (args == "sign_uid.class"))
                next_state = SignState.CHECK;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SignState.ERROR);
            }
            break;
        case SignState.CONFIRM:
            /* need to do check */
            if (status == GPG.StatusCode.GET_LINE && (args == "sign_uid.class"))
                next_state = SignState.CHECK;
            else if (status == GPG.StatusCode.GET_BOOL && (args == "sign_uid.okay"))
                next_state = SignState.CONFIRM;
            /* need to do expire */
            else if (status == GPG.StatusCode.GET_LINE && (args == "sign_uid.expire"))
                next_state = SignState.EXPIRE;
            /* quit */
            else if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = SignState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SignState.ERROR);
            }
            break;
        /* did check, go to confirm */
        case SignState.CHECK:
            if (status == GPG.StatusCode.GET_BOOL && (args == "sign_uid.okay"))
                next_state = SignState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SignState.ERROR);
            }
            break;
        /* quit, go to confirm to save */
        case SignState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = SignState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (SignState.ERROR);
            }
            break;
        /* error, go to quit */
        case SignState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = SignState.QUIT;
            else
                next_state = SignState.ERROR;
            break;

        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (SignState.ERROR);
            break;
    }

    return next_state;
}

public GPGError.ErrorCode sign_uid(Uid uid, Key signer, SignCheck check, SignOptions options) {
    GPG.Key signing_key = signer.get_private();
    g_return_val_if_fail (signing_key, GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    GPG.Key signed_key = seahorse_gpgme_uid_get_pubkey (uid);
    g_return_val_if_fail (signing_key, GPG_E (GPG_ERR_INV_VALUE));

    return sign_process (signed_key, signing_key, uid.actual_index, check, options);
}

public GPGError.ErrorCode sign(Key pkey, Key signer, SignCheck check, SignOptions options) {
    GPG.Key signing_key = signer.get_private();
    g_return_val_if_fail (signing_key, GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    GPG.Key signed_key = pkey.get_public();

    return sign_process (signed_key, signing_key, 0, check, options);
}

private GPGError.ErrorCode sign_process(GPG.Key signed_key, GPG.Key signing_key, uint sign_index, SignCheck check, SignOptions options) {
    GPG.Context ctx = Keyring.new_context(&gerr);
    if (ctx == null)
        return gerr;

        GPGError.ErrorCode gerr = gpgme_signers_add (ctx, signing_key);
        if (!gerr.is_success())
            return gerr;

    SignParm sign_parm;
    sign_parm.index = sign_index;
    sign_parm.expire = ((options & SIGN_EXPIRES) != 0);
    sign_parm.check = check;
    sign_parm.command = "%s%ssign".printf((SIGN_NO_REVOKE in options) ? "nr" : "",
                                          (SIGN_LOCAL in options) ? "l" : "");

    EditParm parms = new EditParm(SIGN_START, sign_action, sign_transit, &sign_parm);

    GPGError.ErrorCode gerr = edit_refresh_gpgme_key(ctx, signed_key, parms);

    return gerr;
}

private enum PassState {
    START,
    COMMAND,
    PASSPHRASE,
    QUIT,
    SAVE,
    ERROR
}

// action helper for changing passphrase
private GPGError.ErrorCode edit_pass_action(PassState state, void* data, int fd) {
    switch (state) {
    case PassState.COMMAND:
        PRINT ((fd, "passwd"));
        break;
    case PassState.PASSPHRASE:
        /* Do nothing */
        return GPG_OK;
    case PassState.QUIT:
        PRINT ((fd, QUIT));
        break;
    case PassState.SAVE:
        PRINT ((fd, YES));
        break;
    default:
        return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for changing passphrase */
private uint edit_pass_transit(PassState current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
    /* start state, go to command */
    case PassState.START:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PassState.COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PassState.ERROR);
        }
        break;

    case PassState.COMMAND:
        /* did command, go to should be the passphrase now */
        if (status == GPG.StatusCode.NEED_PASSPHRASE_SYM)
            next_state = PassState.PASSPHRASE;

        /* If all tries for passphrase were wrong, we get here */
        else if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
            *err = GPG_E (GPG_ERR_BAD_PASSPHRASE);
            next_state = PassState.ERROR;

        /* No idea how we got here ... */
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PassState.ERROR);
        }
        break;
    /* got passphrase now quit */
    case PassState.PASSPHRASE:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PassState.QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PassState.ERROR);
        }
        break;

    /* quit, go to save */
    case PassState.QUIT:
        if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
            next_state = PassState.SAVE;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PassState.ERROR);
        }
        break;

    /* error, go to quit */
    case PassState.ERROR:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PassState.QUIT;
        else
            next_state = PassState.ERROR;
        break;

    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (PassState.ERROR);
        break;
    }

    return next_state;
}

public GPGError.ErrorCode change_pass(Key pkey) {
    g_return_val_if_fail (pkey.usage == Seahorse.Usage.PRIVATE_KEY, GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    EditParm parms = new EditParm(PassState.START, edit_pass_action, edit_pass_transit, null);

    return edit_key(pkey, parms);
}

private enum TrustState {
    START,
    COMMAND,
    VALUE,
    CONFIRM,
    QUIT,
    ERROR,
}

/**
 * Tries to change the owner trust of @pkey to @trust.
 *
 * @param pkey #SeahorseGpgmeKey whose trust will be changed
 * @param trust New trust value that must be at least #Seahorse.Validity.NEVER.
 * If @pkey is a #SeahorseKeyPair, then @trust cannot be #Seahorse.Validity.UNKNOWN.
 * If @pkey is not a #SeahorseKeyPair, then @trust cannot be #Seahorse.Validity.ULTIMATE.
 *
 * Returns: Error value
 **/
public GPGError.ErrorCode set_trust(Key pkey, Seahorse.Validity trust) {
    EditParm *parms;
    int menu_choice;

    debug("set_trust: trust = %i", trust);

    g_return_val_if_fail (trust >= Seahorse.Validity.NEVER, GPG_E (GPG_ERR_INV_VALUE));
    g_return_val_if_fail (seahorse_gpgme_key_get_trust (pkey) != trust, GPG_E (GPG_ERR_INV_VALUE));

    if (pkey.usage == Seahorse.Usage.PRIVATE_KEY) {
        if (trust == Seahorse.Validity.UNKNOWN)
            return GPG_E (GPG_ERR_INV_VALUE);
    } else {
        if (trust == Seahorse.Validity.ULTIMATE)
            return GPG_E (GPG_ERR_INV_VALUE);
    }

    parms = new EditParm(TrustState.START, edit_trust_action, edit_trust_transit, Validity.from_seahorse_validity(trust));
    return edit_key (pkey, parms);
}

/* action helper for setting trust of a key */
private GPGError.ErrorCode edit_trust_action (TrustState state, int trust, int fd) {
    switch (state) {
        /* enter command */
        case TrustState.COMMAND:
            PRINT ((fd, "trust"));
            break;
        /* enter numeric trust value */
        case TrustState.VALUE:
            PRINTF ((fd, "%d", trust));
            break;
        /* confirm ultimate or if save */
        case TrustState.CONFIRM:
            PRINT ((fd, YES));
            break;
        /* quit */
        case TrustState.QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for setting trust of a key */
private uint edit_trust_transit (TrustState current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start state */
        case TrustState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = TrustState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TrustState.ERROR);
            }
            break;
        /* did command, next is value */
        case TrustState.COMMAND:
            if (status == GPG.StatusCode.GET_LINE && (args == "edit_ownertrust.value"))
                next_state = TrustState.VALUE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TrustState.ERROR);
            }
            break;
        /* did value, go to quit or confirm ultimate */
        case TrustState.VALUE:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = TrustState.QUIT;
            else if (status == GPG.StatusCode.GET_BOOL && (args == "edit_ownertrust.set_ultimate.okay"))
                next_state = TrustState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TrustState.ERROR);
            }
            break;
        /* did confirm ultimate, go to quit */
        case TrustState.CONFIRM:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = TrustState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TrustState.ERROR);
            }
            break;
        /* did quit, go to confirm to finish op */
        case TrustState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = TrustState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (TrustState.ERROR);
            }
            break;
        /* error, go to quit */
        case TrustState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = TrustState.QUIT;
            else
                next_state = TrustState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (TrustState.ERROR);
            break;
    }

    return next_state;
}

private enum DisableState {
    START,
    COMMAND,
    QUIT,
    ERROR
}

/**
 * seahorse_gpgme_key_op_set_disabled:
 * @param skey #SeahorseKey to change
 * @param disabled New disabled state
 *
 * Tries to change disabled state of @skey to @disabled.
 *
 * Returns: Error value
 **/
public GPGError.ErrorCode set_disabled(Key pkey, bool disabled) {
    // Get command and op
    string command = disabled ? "disable" : "enable";
    EditParm parms = new EditParm(DisableState.START, edit_disable_action, edit_disable_transit, command);

    return edit_key(pkey, parms);
}

/* action helper for disable/enable a key */
private GPGError.ErrorCode edit_disable_action (DisableState state, gpointer data, int fd) {
    string command = data;

    switch (state) {
        case DisableState.COMMAND:
            PRINT ((fd, command));
            break;
        case DisableState.QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            break;
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for disable/enable a key */
private uint edit_disable_transit (DisableState current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start, do command */
        case DisableState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = DisableState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DisableState.ERROR);
            }
            break;
        /* did command, quit */
        case DisableState.COMMAND:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = DisableState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DisableState.ERROR);
            }
        /* error, quit */
        case DisableState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = DisableState.QUIT;
            else
                next_state = DisableState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DisableState.ERROR);
            break;
    }

    return next_state;
}

private struct ExpireParm {
    uint    index;
    time_t    expires;
}

private enum ExpireState {
    START,
    SELECT,
    COMMAND,
    DATE,
    QUIT,
    SAVE,
    ERROR
}

/* action helper for changing expiration date of a key */
private GPGError.ErrorCode edit_expire_action (ExpireState state, ExpireParm parm, int fd) {
    switch (state) {
        /* selected key */
        case ExpireState.SELECT:
            PRINTF ((fd, "key %d", parm.index));
            break;
        case ExpireState.COMMAND:
            PRINT ((fd, "expire"));
            break;
        /* set date */
        case ExpireState.DATE:
            PRINT ((fd, (parm.expires) ?
                Seahorse.Util.get_date_string (parm.expires) : "0"));
            break;
        case ExpireState.QUIT:
            PRINT ((fd, QUIT));
            break;
        case ExpireState.SAVE:
            PRINT ((fd, YES));
            break;
        case ExpireState.ERROR:
            break;
        default:
            return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for changing expiration date of a key */
private uint edit_expire_transit (ExpireState current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start state, selected key */
        case ExpireState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = ExpireState.SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ExpireState.ERROR);
            }
            break;
        /* selected key, do command */
        case ExpireState.SELECT:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = ExpireState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ExpireState.ERROR);
            }
            break;
        /* did command, set expires */
        case ExpireState.COMMAND:
            if (status == GPG.StatusCode.GET_LINE && (args == "keygen.valid"))
                next_state = ExpireState.DATE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ExpireState.ERROR);
            }
            break;
        /* set expires, quit */
        case ExpireState.DATE:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = ExpireState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ExpireState.ERROR);
            }
            break;
        /* quit, save */
        case ExpireState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = ExpireState.SAVE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (ExpireState.ERROR);
            }
            break;
        /* error, quit */
        case ExpireState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = ExpireState.QUIT;
            else
                next_state = ExpireState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (ExpireState.ERROR);
            break;
    }
    return next_state;
}

public GPGError.ErrorCode set_expires(SubKey subkey, time_t expires) {
    g_return_val_if_fail (expires != (time_t)seahorse_pgp_subkey_get_expires (SEAHORSE_PGP_SUBKEY (subkey)), GPG_E (GPG_ERR_INV_VALUE));

    GPG.Key key = subkey.get_pubkey();
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    ExpireParm exp_parm;
    exp_parm.index = subkey.get_index();
    exp_parm.expires = expires;

    EditParm parms = new EditParm(ExpireState.START, edit_expire_action, edit_expire_transit, &exp_parm);
    return edit_refresh_gpgme_key (null, key, parms);
}

private enum AddRevokerState {
    START,
    COMMAND,
    SELECT,
    CONFIRM,
    QUIT,
    ERROR
}

/* action helper for adding a revoker */
private GPGError.ErrorCode add_revoker_action (AddRevokerState state, string keyid, int fd) {
    switch (state) {
        case AddRevokerState.COMMAND:
            PRINT ((fd, "addrevoker"));
            break;
        /* select revoker */
        case AddRevokerState.SELECT:
            PRINT ((fd, keyid));
            break;
        case AddRevokerState.CONFIRM:
            PRINT ((fd, YES));
            break;
        case AddRevokerState.QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for adding a revoker */
private uint add_revoker_transit (uint current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start, do command */
        case AddRevokerState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddRevokerState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddRevokerState.ERROR);
            }
            break;
        /* did command, select revoker */
        case AddRevokerState.COMMAND:
            if (status == GPG.StatusCode.GET_LINE && (args == "keyedit.add_revoker"))
                next_state = AddRevokerState.SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddRevokerState.ERROR);
            }
            break;
        /* selected revoker, confirm */
        case AddRevokerState.SELECT:
            if (status == GPG.StatusCode.GET_BOOL && (args == "keyedit.add_revoker.okay"))
                next_state = AddRevokerState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddRevokerState.ERROR);
            }
            break;
        /* confirmed, quit */
        case AddRevokerState.CONFIRM:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddRevokerState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddRevokerState.ERROR);
            }
            break;
        /* quit, confirm(=save) */
        case AddRevokerState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = AddRevokerState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddRevokerState.ERROR);
            }
            break;
        /* error, quit */
        case AddRevokerState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddRevokerState.QUIT;
            else
                next_state = AddRevokerState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (AddRevokerState.ERROR);
            break;
    }

    return next_state;
}

public GPGError.ErrorCode add_revoker (Key pkey, Key revoker) {
    if (pkey.usage != Seahorse.Usage.PRIVATE_KEY || revoker.usage != Seahorse.Usage.PRIVATE_KEY)
        return GPG_E (GPG_ERR_WRONG_KEY_USAGE);

    string? keyid = pkey.keyid;
    g_return_val_if_fail (keyid, GPG_E (GPG_ERR_INV_VALUE));

    EditParm *parms;
    parms = new EditParm(AddRevokerState.START, add_revoker_action, add_revoker_transit, keyid);

    return edit_key (pkey, parms);
}

private enum AddUidState {
    START,
    COMMAND,
    NAME,
    EMAIL,
    COMMENT,
    QUIT,
    SAVE,
    ERROR
}

private struct UidParm {
    string name;
    string email;
    string comment;
}

/* action helper for adding a new user ID */
private GPGError.ErrorCode add_uid_action(AddUidState state, UidParm parm, int fd) {
    switch (state) {
        case AddUidState.COMMAND:
            PRINT ((fd, "adduid"));
            break;
        case AddUidState.NAME:
            PRINT ((fd, parm.name));
            break;
        case AddUidState.EMAIL:
            PRINT ((fd, parm.email));
            break;
        case AddUidState.COMMENT:
            PRINT ((fd, parm.comment));
            break;
        case AddUidState.QUIT:
            PRINT ((fd, QUIT));
            break;
        case AddUidState.SAVE:
            PRINT ((fd, YES));
            break;
        default:
            return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for adding a new user ID */
private uint add_uid_transit(uint current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start, do command */
        case AddUidState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddUidState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddUidState.ERROR);
            }
            break;
        /* did command, do name */
        case AddUidState.COMMAND:
            if (status == GPG.StatusCode.GET_LINE && (args == "keygen.name"))
                next_state = AddUidState.NAME;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddUidState.ERROR);
            }
            break;
        /* did name, do email */
        case AddUidState.NAME:
            if (status == GPG.StatusCode.GET_LINE && (args == "keygen.email"))
                next_state = AddUidState.EMAIL;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddUidState.ERROR);
            }
            break;
        /* did email, do comment */
        case AddUidState.EMAIL:
            if (status == GPG.StatusCode.GET_LINE && (args == "keygen.comment"))
                next_state = AddUidState.COMMENT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddUidState.ERROR);
            }
            break;
        /* did comment, quit */
        case AddUidState.COMMENT:
            next_state = AddUidState.QUIT;
            break;
        /* quit, save */
        case AddUidState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = AddUidState.SAVE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddUidState.ERROR);
            }
            break;
        /* error, quit */
        case AddUidState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddUidState.QUIT;
            else
                next_state = AddUidState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (AddUidState.ERROR);
            break;
    }

    return next_state;
}

public GPGError.ErrorCode add_uid (Key pkey, string name, string? email, string? comment) {
    if (pkey.usage != Seahorse.Usage.PRIVATE_KEY)
        return GPG_E (GPG_ERR_WRONG_KEY_USAGE);
    if (name == null && name < 5)
        return GPG_E (GPG_ERR_INV_VALUE);

    EditParm *parms;
    UidParm *uid_parm;
    uid_parm = g_new (UidParm, 1);
    uid_parm.name = name;
    uid_parm.email = email;
    uid_parm.comment = comment;

    parms = new EditParm(AddUidState.START, add_uid_action, add_uid_transit, uid_parm);

    return edit_key(pkey, parms);
}

private enum AddKeyState {
    START,
    COMMAND,
    TYPE,
    LENGTH,
    EXPIRES,
    QUIT,
    SAVE,
    ERROR
}

private struct SubkeyParm {
    uint type;
    uint length;
    time_t expires;
}

/* action helper for adding a subkey */
private GPGError.ErrorCode add_key_action (AddKeyState state, SubkeyParm parm, int fd) {
    switch (state) {
        case AddKeyState.COMMAND:
            PRINT ((fd, "addkey"));
            break;
        case AddKeyState.TYPE:
            PRINTF ((fd, "%d", parm.type));
            break;
        case AddKeyState.LENGTH:
            PRINTF ((fd, "%d", parm.length));
            break;
        /* Get exact date or 0 */
        case AddKeyState.EXPIRES:
            PRINT ((fd, (parm.expires) ?
                Seahorse.Util.get_date_string (parm.expires) : "0"));
            break;
        case AddKeyState.QUIT:
            PRINT ((fd, QUIT));
            break;
        case AddKeyState.SAVE:
            PRINT ((fd, YES));
            break;
        default:
            return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for adding a subkey */
private uint add_key_transit(AddKeyState current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start, do command */
        case AddKeyState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddKeyState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddKeyState.ERROR);
            }
            break;
        /* did command, do type */
        case AddKeyState.COMMAND:
        case AddKeyState.TYPE:
        case AddKeyState.LENGTH:
        case AddKeyState.EXPIRES:
            if (status == GPG.StatusCode.GET_LINE && (args == "keygen.algo"))
                next_state = AddKeyState.TYPE;
            else if (status == GPG.StatusCode.GET_LINE && (args == "keygen.size"))
                next_state = AddKeyState.LENGTH;
            else if (status == GPG.StatusCode.GET_LINE && (args == "keygen.valid"))
                next_state = AddKeyState.EXPIRES;
            else if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddKeyState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                return AddKeyState.ERROR; /* Legitimate errors error here */
            }
            break;
        /* quit, save */
        case AddKeyState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = AddKeyState.SAVE;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (AddKeyState.ERROR);
            }
            break;
        /* error, quit */
        case AddKeyState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = AddKeyState.QUIT;
            else
                next_state = AddKeyState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (AddKeyState.ERROR);
            break;
    }

    return next_state;
}

public GPGError.ErrorCode add_subkey(Key pkey, SeahorseKeyEncType type, uint length, time_t expires) {
    EditParm *parms;
    SubkeyParm *key_parm;
    uint real_type;
    SeahorseKeyTypeTable table;
    GPGError.ErrorCode gerr;

    g_return_val_if_fail (pkey.get_usage() == Seahorse.Usage.PRIVATE_KEY, GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    gerr = seahorse_gpgme_get_keytype_table (&table);
    g_return_val_if_fail (gerr.is_success(), gerr);

    /* Check length range & type */
    switch (type) {
        case DSA:
            real_type = table.dsa_sign;
            g_return_val_if_fail (length >= DSA_MIN && length <= DSA_MAX, GPG_E (GPG_ERR_INV_VALUE));
            break;
        case ELGAMAL:
            real_type = table.elgamal_enc;
            g_return_val_if_fail (length >= ELGAMAL_MIN && length <= LENGTH_MAX, GPG_E (GPG_ERR_INV_VALUE));
            break;
        case RSA_SIGN: case RSA_ENCRYPT:
            if (type == RSA_SIGN)
                real_type = table.rsa_sign;
            else
                real_type = table.rsa_enc;
            g_return_val_if_fail (length >= RSA_MIN && length <= LENGTH_MAX, GPG_E (GPG_ERR_INV_VALUE));
            break;
        default:
            g_return_val_if_reached (GPG_E (GPG_ERR_INV_VALUE));
            break;
    }

    key_parm = SubkeyParm();
    key_parm.type = real_type;
    key_parm.length = length;
    key_parm.expires = expires;

    parms = new EditParm(AddKeyState.START, add_key_action, add_key_transit, key_parm);

    return edit_key (pkey, parms);
}

private enum DelKeyState {
    START,
    SELECT,
    COMMAND,
    CONFIRM,
    QUIT,
    ERROR
}

/* action helper for deleting a subkey */
private GPGError.ErrorCode del_key_action(DelKeyState state, gpointer data, int fd) {
    switch (state) {
        /* select key */
        case DelKeyState.SELECT:
            PRINTF ((fd, "key %d", GPOINTER_TO_UINT (data)));
            break;
        case DelKeyState.COMMAND:
            PRINT ((fd, "delkey"));
            break;
        case DelKeyState.CONFIRM:
            PRINT ((fd, YES));
            break;
        case DelKeyState.QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            return GPG_E (GPG_ERR_GENERAL);
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for deleting a subkey */
private uint del_key_transit(DelKeyState current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start, select key */
        case DelKeyState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = DelKeyState.SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DelKeyState.ERROR);
            }
            break;
        /* selected key, do command */
        case DelKeyState.SELECT:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = DelKeyState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DelKeyState.ERROR);
            }
            break;
        case DelKeyState.COMMAND:
            /* did command, confirm */
            if (status == GPG.StatusCode.GET_BOOL && args == "keyedit.remove.subkey.okay")
                next_state = DelKeyState.CONFIRM;
            /* did command, quit */
            else if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = DelKeyState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DelKeyState.ERROR);
            }
            break;
        /* confirmed, quit */
        case DelKeyState.CONFIRM:
            next_state = DelKeyState.QUIT;
            break;
        /* quit, confirm(=save) */
        case DelKeyState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = DelKeyState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (DelKeyState.ERROR);
            }
            break;
        /* error, quit */
        case DelKeyState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = DelKeyState.QUIT;
            else
                next_state = DelKeyState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DelKeyState.ERROR);
            break;
    }

    return next_state;
}

public GPGError.ErrorCode del_subkey(SubKey subkey) {
    GPG.Key key = seahorse_gpgme_subkey_get_pubkey (subkey);
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    int index = subkey.get_index();
    EditParm parms = new EditParm(DelKeyState.START, del_key_action,
                                    del_key_transit, uint_TO_POINTER (index));

    return edit_refresh_gpgme_key (null, key, parms);
}

private struct RevSubkeyParm {
    uint index;
    RevokeReason reason;
    string description;
}

private enum RevSubkeyState {
    START,
    SELECT,
    COMMAND,
    CONFIRM,
    REASON,
    DESCRIPTION,
    ENDDESC,
    QUIT,
    ERROR
}

/* action helper for revoking a subkey */
private GPGError.ErrorCode rev_subkey_action (RevSubkeyState state, RevSubkeyParm parm, int fd) {
    switch (state) {
        case RevSubKeyState.SELECT:
            PRINTF ((fd, "key %d", parm.index));
            break;
        case RevSubKeyState.COMMAND:
            PRINT ((fd, "revkey"));
            break;
        case RevSubKeyState.CONFIRM:
            PRINT ((fd, YES));
            break;
        case RevSubKeyState.REASON:
            PRINTF ((fd, "%d", parm.reason));
            break;
        case RevSubKeyState.DESCRIPTION:
            PRINTF ((fd, "%s", parm.description));
            break;
        case RevSubKeyState.ENDDESC:
            /* Need empty line, which is written at the end */
            break;
        case RevSubKeyState.QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
    }

    PRINT ((fd, "\n"));
    return GPG_OK;
}

/* transition helper for revoking a subkey */
private uint rev_subkey_transit(uint current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
        /* start, select key */
        case RevSubKeyState.START:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = RevSubKeyState.SELECT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        /* selected key, do command */
        case RevSubKeyState.SELECT:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = RevSubKeyState.COMMAND;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        /* did command, confirm */
        case RevSubKeyState.COMMAND:
            if (status == GPG.StatusCode.GET_BOOL && (args == "keyedit.revoke.subkey.okay"))
                next_state = RevSubKeyState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        case RevSubKeyState.CONFIRM:
            /* did confirm, do reason */
            if (status == GPG.StatusCode.GET_LINE && (args == "ask_revocation_reason.code"))
                next_state = RevSubKeyState.REASON;
            /* did confirm, quit */
            else if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = RevSubKeyState.QUIT;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        /* did reason, do description */
        case RevSubKeyState.REASON:
            if (status == GPG.StatusCode.GET_LINE && (args == "ask_revocation_reason.text"))
                next_state = RevSubKeyState.DESCRIPTION;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        case RevSubKeyState.DESCRIPTION:
            /* did description, end it */
            if (status == GPG.StatusCode.GET_LINE && (args == "ask_revocation_reason.text"))
                next_state = RevSubKeyState.ENDDESC;
            /* did description, confirm */
            else if (status == GPG.StatusCode.GET_BOOL && (args == "ask_revocation_reason.okay"))
                next_state = RevSubKeyState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        /* ended description, confirm */
        case RevSubKeyState.ENDDESC:
            if (status == GPG.StatusCode.GET_BOOL && (args == "ask_revocation_reason.okay"))
                next_state = RevSubKeyState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        /* quit, confirm(=save) */
        case RevSubKeyState.QUIT:
            if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
                next_state = RevSubKeyState.CONFIRM;
            else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (RevSubKeyState.ERROR);
            }
            break;
        /* error, quit */
        case RevSubKeyState.ERROR:
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
                next_state = RevSubKeyState.QUIT;
            else
                next_state = RevSubKeyState.ERROR;
            break;
        default:
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (RevSubKeyState.ERROR);
            break;
    }

    return next_state;
}

public GPGError.ErrorCode revoke_subkey(SubKey subkey, RevokeReason reason, string description) {
    GPG.SubKey gsubkey = subkey.get_subkey();
    g_return_val_if_fail (!gsubkey.revoked, GPG_E (GPG_ERR_INV_VALUE));

    GPG.Key key = subkey.get_pubkey();
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    RevSubkeyParm rev_parm;
    rev_parm.index = subkey.get_index();
    rev_parm.reason = reason;
    rev_parm.description = description;

    EditParm parms = new EditParm(RevSubKeyState.START, rev_subkey_action,
                                    rev_subkey_transit, &rev_parm);

    return edit_refresh_gpgme_key (null, key, parms);
}

private struct PrimaryParm {
    uint index;
}

private enum PrimaryState {
    START,
    SELECT,
    COMMAND,
    QUIT,
    SAVE,
    ERROR
}

/* action helper for setting primary uid */
private GPGError.ErrorCode primary_action(PrimaryState state, PrimaryParm parm, int fd) {
    switch (state) {
    case PrimaryState.SELECT:
        /* Note that the GPG id is not 0 based */
        PRINTF ((fd, "uid %d", parm.index));
        break;
    case PrimaryState.COMMAND:
        PRINT ((fd, "primary"));
        break;
    case PrimaryState.QUIT:
        PRINT ((fd, QUIT));
        break;
    case PrimaryState.SAVE:
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
private uint primary_transit(uint current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {

    /* start, select key */
    case PrimaryState.START:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PrimaryState.SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PrimaryState.ERROR);
        }
        break;

    /* selected key, do command */
    case PrimaryState.SELECT:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PrimaryState.COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PrimaryState.ERROR);
        }
        break;

    /* did command, quit */
    case PrimaryState.COMMAND:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PrimaryState.QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PrimaryState.ERROR);
        }
        break;

    /* quitting so save */
    case PrimaryState.QUIT:
        if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
            next_state = PrimaryState.SAVE;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PrimaryState.ERROR);
        }
        break;

    /* error, quit */
    case PrimaryState.ERROR:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PrimaryState.QUIT;
        else
            next_state = PrimaryState.ERROR;
        break;

    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (PrimaryState.ERROR);
        break;
    }

    return next_state;
}

public GPGError.ErrorCode primary_uid(Uid uid) {
    PrimaryParm pri_parm;
    EditParm *parms;

    g_return_val_if_fail (SEAHORSE_IS_GPGME_UID (uid), GPG_E (GPG_ERR_WRONG_KEY_USAGE));

    /* Make sure not revoked */
    GPG.UserID userid = seahorse_gpgme_uid_get_userid (uid);
    g_return_val_if_fail (userid != null && !userid.revoked && !userid.invalid,
                          GPG_E (GPG_ERR_INV_VALUE));

    GPG.Key key = seahorse_gpgme_uid_get_pubkey (uid);
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    pri_parm.index = seahorse_gpgme_uid_get_actual_index (uid);

    parms = new EditParm(PrimaryState.START, primary_action,
                                    primary_transit, &pri_parm);

    return edit_refresh_gpgme_key (null, key, parms);
}


private struct DelUidParm {
    uint index;
}

private enum DelUidState {
    START,
    SELECT,
    COMMAND,
    CONFIRM,
    QUIT,
    SAVE,
    ERROR,
}

/* action helper for removing a uid */
private GPGError.ErrorCode del_uid_action (uint state, DelUidParm parm, int fd) {
    switch (state) {
    case DelUidState.SELECT:
        PRINTF ((fd, "uid %d", parm.index));
        break;
    case DelUidState.COMMAND:
        PRINT ((fd, "deluid"));
        break;
    case DelUidState.CONFIRM:
        PRINT ((fd, YES));
        break;
    case DelUidState.QUIT:
        PRINT ((fd, QUIT));
        break;
    case DelUidState.SAVE:
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
private uint del_uid_transit(uint current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {

    /* start, select key */
    case DelUidState.START:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = DelUidState.SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DelUidState.ERROR);
        }
        break;

    /* selected key, do command */
    case DelUidState.SELECT:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = DelUidState.COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DelUidState.ERROR);
        }
        break;

    /* did command, confirm */
    case DelUidState.COMMAND:
        if (status == GPG.StatusCode.GET_BOOL && (args == "keyedit.remove.uid.okay"))
            next_state = DelUidState.CONFIRM;
        else if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = DelUidState.QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (RevSubKeyState.ERROR);
        }
        break;

    /* confirmed, quit */
    case DelUidState.CONFIRM:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = DelUidState.QUIT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (DelUidState.ERROR);
        }
        break;

    /* quitted so save */
    case DelUidState.QUIT:
        if (status == GPG.StatusCode.GET_BOOL && (args == SAVE))
            next_state = DelUidState.SAVE;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (RevSubKeyState.ERROR);
        }
        break;

    /* error, quit */
    case DelUidState.ERROR:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = DelUidState.QUIT;
        else
            next_state = DelUidState.ERROR;
        break;

    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (DelUidState.ERROR);
        break;
    }

    return next_state;
}

public GPGError.ErrorCode del_uid(Uid uid) {
    GPG.Key key = uid.get_pubkey();
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    DelUidParm del_uid_parm;
    del_uid_parm.index = uid.get_actual_index();

    EditParm parms = new EditParm(DelUidState.START, del_uid_action,
                                    del_uid_transit, &del_uid_parm);

    return edit_refresh_gpgme_key (null, key, parms);
}

private struct PhotoIdAddParm {
    string filename;
}

private enum PhotoIdAddState {
    START,
    COMMAND,
    URI,
    BIG,
    QUIT,
    SAVE,
    ERROR,
}

public GPGError.ErrorCode photo_add(Key pkey, string filename) {
    g_return_val_if_fail (filename, GPG_E (GPG_ERR_INV_VALUE));

    GPG.Key key = pkey.get_public();
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    PhotoIdAddParm photoid_add_parm;
    photoid_add_parm.filename = filename;

    EditParm parms = new EditParm(PhotoIdAddState.START, photoid_add_action,
                                    photoid_add_transit, &photoid_add_parm);

    return edit_refresh_gpgme_key (null, key, parms);
}

public GPGError.ErrorCode photo_delete(Photo photo) {
    GPG.Key key = photo.get_pubkey();
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    DelUidParm del_uid_parm;
    del_uid_parm.index = photo.get_index();

    EditParm parms = new EditParm(DelUidState.START, del_uid_action, del_uid_transit, &del_uid_parm);

    return edit_refresh_gpgme_key (null, key, parms);
}

/* action helper for adding a photoid to a #SeahorseKey */
private GPGError.ErrorCode photoid_add_action(PhotoIdAddState state, PhotoIdAddParm parm, int fd) {
    switch (state) {
    case PhotoIdAddState.COMMAND:
        PRINT ((fd, "addphoto"));
        break;
    case PhotoIdAddState.URI:
        PRINT ((fd, parm.filename));
        break;
    case PhotoIdAddState.BIG:
        PRINT ((fd, YES));
        break;
    case PhotoIdAddState.QUIT:
        PRINT ((fd, QUIT));
        break;
    case PhotoIdAddState.SAVE:
        PRINT ((fd, YES));
        break;
    default:
        g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
        break;
    }

    Seahorse.Util.print_fd (fd, "\n");
    return GPG_OK;
}

private uint photoid_add_transit(uint current_state, GPG.StatusCode status, string args, gpointer data, GPGError.ErrorCode *err) {
    uint next_state;

    switch (current_state) {
    case PhotoIdAddState.START:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PhotoIdAddState.COMMAND;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdAddState.ERROR);
        }
        break;
    case PhotoIdAddState.COMMAND:
        if (status == GPG.StatusCode.GET_LINE && (args == "photoid.jpeg.add")) {
            next_state = PhotoIdAddState.URI;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdAddState.ERROR);
        }
        break;
   case PhotoIdAddState.URI:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
            next_state = PhotoIdAddState.QUIT;
        } else if (status == GPG.StatusCode.GET_BOOL && (args == "photoid.jpeg.size")) {
            next_state = PhotoIdAddState.BIG;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdAddState.ERROR);
        }
        break;
    case PhotoIdAddState.BIG:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
            next_state = PhotoIdAddState.QUIT;
        /* This happens when the file is invalid or can't be accessed */
        } else if (status == GPG.StatusCode.GET_LINE && (args == "photoid.jpeg.add")) {
            *err = GPG_E (GPG_ERR_USER_1);
            return PhotoIdAddState.ERROR;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdAddState.ERROR);
        }
        break;
    case PhotoIdAddState.QUIT:
        if (status == GPG.StatusCode.GET_BOOL && (args == SAVE)) {
            next_state = PhotoIdAddState.SAVE;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdAddState.ERROR);
        }
        break;
    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (PhotoIdAddState.ERROR);
        break;
    }

    return next_state;
}

private struct PhotoIdLoadParm {
    GList *photos;
    uint uid;
    uint num_uids;
    char *output_file;
    GPG.Key key;
}

private enum PhotoIdLoadState {
    START,
    SELECT,
    OUTPUT_IMAGE,
    DESELECT,
    QUIT,
    ERROR,
}

public GPGError.ErrorCode photos_load(Key pkey) {
    // Make sure there's enough room for the .jpg extension
    const string image_path = "/tmp/seahorse-photoid-XXXXXX\0\0\0\0";

    GPG.Key key = pkey.get_public();
    if (key == null || key.subkeys == null || key.subkeys.keyid == null)
        return GPG_E(GPG_ERR_INV_VALUE);
    string keyid = key.subkeys.keyid;

    debug ("PhotoIDLoad Start");

    GPGError.ErrorCode gerr;
    int fd = g_mkstemp (image_path);
    if(fd == -1) {
        gerr = GPG_E(GPG_ERR_GENERAL);
    } else {
        g_unlink(image_path);
        close(fd);
        strcat (image_path, ".jpg");

        PhotoIdLoadParm photoid_load_parm;
        photoid_load_parm.uid = 1;
        photoid_load_parm.num_uids = 0;
        photoid_load_parm.photos = null;
        photoid_load_parm.output_file = image_path;
        photoid_load_parm.key = key;

        debug("PhotoIdLoad KeyID %s", keyid);
        gerr = Operation.num_uids(null, keyid, out (photoid_load_parm.num_uids));
        debug("PhotoIDLoad Number of UIDs %i", photoid_load_parm.num_uids);

        if (gerr.is_success()) {
            setenv ("SEAHORSE_IMAGE_FILE", image_path, 1);
            string oldpath = getenv("PATH");

            string path = "%s:%s".printf(Config.EXECDIR, getenv ("PATH"));
            setenv ("PATH", path, 1);

            EditParm parms = new EditParm(PhotoIdLoadState.START, photoid_load_action,
                                            photoid_load_transit, &photoid_load_parm);

            /* generate list */
            gerr = edit_gpgme_key (null, key, parms);
            setenv ("PATH", oldpath, 1);

            if (gerr.is_success())
                pkey.photos = photoid_load_parm.photos;
        }
    }

    debug ("PhotoIDLoad Done");

    return gerr;
}

public GPGError.ErrorCode photo_primary(Photo photo) {
    GPG.Key key = photo.get_pubkey();
    g_return_val_if_fail (key, GPG_E (GPG_ERR_INV_VALUE));

    PrimaryParm pri_parm;
    pri_parm.index = photo.get_index();

    EditParm parms = new EditParm(PrimaryState.START, primary_action, primary_transit, &pri_parm);

    return edit_refresh_gpgme_key (null, key, parms);
}

/* action helper for getting a list of photoids attached to a #SeahorseKey */
private GPGError.ErrorCode photoid_load_action(PhotoIdLoadState state, PhotoIdLoadParm parm, int fd) {
    switch (state) {
        case PhotoIdLoadState.SELECT:
            PRINTF ((fd, "uid %d", parm.uid));
            break;
        case PhotoIdLoadState.OUTPUT_IMAGE:
            PRINT ((fd, "showphoto"));
            break;
        case PhotoIdLoadState.DESELECT:
            PRINTF ((fd, "uid %d", parm.uid));
            break;
        case PhotoIdLoadState.QUIT:
            PRINT ((fd, QUIT));
            break;
        default:
            g_return_val_if_reached (GPG_E (GPG_ERR_GENERAL));
            break;
    }

    Seahorse.Util.print_fd(fd, "\n");
    return GPG_OK;
}

private uint photoid_load_transit(PhotoIdLoadState current_state, GPG.StatusCode status, string args, PhotoIdLoadParm parm, GPGError.ErrorCode *err) {
    uint next_state = 0;
    Posix.stat st;
    GError *error = null;

    switch (current_state) {
    /* start, get photoid list */
    case PhotoIdLoadState.START:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT))
            next_state = PhotoIdLoadState.SELECT;
        else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdLoadState.ERROR);
        }
        break;

    case PhotoIdLoadState.SELECT:
        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
            next_state = PhotoIdLoadState.OUTPUT_IMAGE;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdLoadState.ERROR);
        }
        break;

    case PhotoIdLoadState.OUTPUT_IMAGE:

        if (FileUtils.test(parm.output_file, G_FILE_TEST_EXISTS)) {
            Photo photo = new Photo(parm.key, null, parm.uid);
            parm.photos = g_list_append (parm.photos, photo);
            GdkPixbuf *pixbuf = null;

            if (g_stat (parm.output_file, &st) == -1) {
                warning("couldn't stat output image file '%s': %s", parm.output_file, g_strerror (errno));
            } else if (st.st_size > 0) {
                try {
                    pixbuf = new Gdk.Pixbuf.from_file(parm.output_file);
                } catch (GLib.Error e) {
                    warning("Loading image %s failed: %s", parm.output_file, e.message ?? "unknown");
                }
            }
            g_unlink (parm.output_file);

            // Load a 'missing' icon
            if (!pixbuf) {
                pixbuf = Gtk.IconTheme.get_default().load_icon("gnome-unknown", 48, 0, null);
            }

            photo.pixbuf = pixbuf;
        }

        if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
            next_state = PhotoIdLoadState.DESELECT;
        } else {
            *err = GPG_E (GPG_ERR_GENERAL);
            g_return_val_if_reached (PhotoIdLoadState.ERROR);
        }
        break;

    case PhotoIdLoadState.DESELECT:
        if (parm.uid < parm.num_uids) {
            parm.uid = parm.uid + 1;
            debug("PhotoIDLoad Next UID %i", parm.uid);
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
                next_state = PhotoIdLoadState.SELECT;
            } else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PhotoIdLoadState.ERROR);
            }
        } else {
            if (status == GPG.StatusCode.GET_LINE && (args == PROMPT)) {
                next_state = PhotoIdLoadState.QUIT;
                debug("PhotoIDLoad Quiting Load");
            } else {
                *err = GPG_E (GPG_ERR_GENERAL);
                g_return_val_if_reached (PhotoIdLoadState.ERROR);
            }
        }
        break;

    case PhotoIdLoadState.QUIT:
        /* Shouldn't be reached */
        *err = GPG_E (GPG_ERR_GENERAL);
        debug("PhotoIDLoad Reached Quit");
        g_return_val_if_reached (PhotoIdLoadState.ERROR);
        break;

    default:
        *err = GPG_E (GPG_ERR_GENERAL);
        g_return_val_if_reached (PhotoIdLoadState.ERROR);
        break;
    }

    return next_state;
}
}

}
