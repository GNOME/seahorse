/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/* #undef G_LOG_DOMAIN */
/* #define G_LOG_DOMAIN "operation" */

/**
 * SECTION:seahorse-gpgme
 * @short_description: gpgme specific error and data conversion functions
 * @include:seahorse-gpgme.h
 *
 **/

/**
 * seahorse_gpgme_error_domain:
 *
 *
 * Returns: A Quark with the content "seahorse-gpgme-error"
 */
private GQuark q = 0;
public Quark seahorse_gpgme_error_domain() {
    if (q == 0)
        q = g_quark_from_static_string ("seahorse-gpgme-error");
    return q;
}

public bool seahorse_gpgme_propagate_error(GPGError.ErrorCode gerr, GError** error) {
    gpgme_err_code_t code;

    /* Make sure this is actually an error */
    code = gpgme_err_code (gerr);
    if (code == 0)
        return FALSE;

    /* Special case some error messages */
    switch (code) {
    case GPG_ERR_DECRYPT_FAILED:
        g_set_error_literal (error, SEAHORSE_GPGME_ERROR, code,
                     _("Decryption failed. You probably do not have the decryption key."));
        break;
    case GPG_ERR_CANCELED:
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                             _("The operation was cancelled"));
        break;
    default:
        g_set_error_literal (error, SEAHORSE_GPGME_ERROR, code,
                             gpgme_strerror (gerr));
        break;
    }

    return TRUE;
}

/**
 * Acquires an additional gpgme reference for the key
 *
 * @key: the gpgme key
 *
 * Returns: the key
 */
private GPG.Key ref_return_key (GPG.Key key) {
    gpgme_key_ref (key);
    return key;
}

/**
 * Creates a new boxed type "GPG.Key"
 *
 * Returns: the new boxed type
 */
private GType type = 0;
public GType seahorse_gpgme_boxed_key_type() {
    if (!type)
        type = g_boxed_type_register_static ("GPG.Key", ref_return_key, gpgme_key_unref);
    return type;
}

/**
 * converts the gpgme validity to the seahorse validity
 *
 * @validity: the gpgme validity of a key
 *
 * Returns: The seahorse validity
 */
SeahorseValidity seahorse_gpgme_convert_validity (GPG.Validity validity) {
    switch (validity) {
        case GPG.Validity.NEVER:
            return Seahorse.Validity.NEVER;
        case GPG.Validity.MARGINAL:
            return Seahorse.Validity.MARGINAL;
        case GPG.Validity.FULL:
            return Seahorse.Validity.FULL;
        case GPG.Validity.ULTIMATE:
            return Seahorse.Validity.ULTIMATE;
        case GPG.Validity.UNDEFINED:
        case GPG.Validity.UNKNOWN:
        default:
            return Seahorse.Validity.UNKNOWN;
    }
}

struct SeahorseKeyTypeTable {
	int rsa_sign;
    int rsa_enc;
    int dsa_sign;
    int elgamal_enc;
}

/*
 * Based on the values in ask_algo() in gnupg's g10/keygen.c
 * http://cvs.gnupg.org/cgi-bin/viewcvs.cgi/trunk/g10/keygen.c?rev=HEAD&root=GnuPG&view=log
_ */
const SeahorseKeyTypeTable KEYTYPES_2012 = SeahorseKeyTypeTable()
    { rsa_sign=4, rsa_enc=6, dsa_sign=3, elgamal_enc=5 };
const SeahorseKeyTypeTable KEYTYPES_140 = SeahorseKeyTypeTable()
    { rsa_sign=5, rsa_enc=6, dsa_sign=2, elgamal_enc=4 };
const SeahorseKeyTypeTable KEYTYPES_124 = SeahorseKeyTypeTable()
    { rsa_sign=4, rsa_enc=5, dsa_sign=2, elgamal_enc=3 };
const SeahorseKeyTypeTable KEYTYPES_120 = SeahorseKeyTypeTable()
    { rsa_sign=5, rsa_enc=6, dsa_sign=2, elgamal_enc=3 };

const Seahorse.Version VER_2012 = seahorse_util_version(2,0,12,0);
const Seahorse.Version VER_190  = seahorse_util_version(1,9,0,0);
const Seahorse.Version VER_1410 = seahorse_util_version(1,4,10,0);
const Seahorse.Version VER_140  = seahorse_util_version(1,4,0,0);
const Seahorse.Version VER_124  = seahorse_util_version(1,2,4,0);
const Seahorse.Version VER_120  = seahorse_util_version(1,2,0,0);

/**
 * Based on the gpg version in use, sets @table
 * to contain the numbers that gpg uses in its CLI
 * for adding new subkeys. This tends to get broken
 * at random by new versions of gpg, but there's no good
 * API for this.
 *
 * @param table The requested keytype table
 *
 * @return GPG_ERR_USER_2 if gpg is too old.
 **/
public GPGError.ErrorCode seahorse_gpgme_get_keytype_table (out SeahorseKeyTypeTable table) {
    GPG.EngineInfo engine;
    GPGError.ErrorCode gerr = gpgme_get_engine_info (out engine);
    g_return_val_if_fail (gerr.is_success(), gerr);

    while (engine && engine->protocol != GPGME_PROTOCOL_OpenPGP)
        engine = engine->next;
    g_return_val_if_fail (engine != null, GPG_E (GPG_ERR_GENERAL));

    Seahorse.Version ver = seahorse_util_parse_version (engine->version);

    if (ver >= VER_2012 || (ver >= VER_1410 && ver < VER_190))
        *table = (SeahorseKeyTypeTable)&KEYTYPES_2012;
    else if (ver >= VER_140 || ver >= VER_190)
        *table = (SeahorseKeyTypeTable)&KEYTYPES_140;
    else if (ver >= VER_124)
        *table = (SeahorseKeyTypeTable)&KEYTYPES_124;
    else if (ver >= VER_120)
        *table = (SeahorseKeyTypeTable)&KEYTYPES_120;
    else    // older versions not supported
        gerr = GPG_E (GPG_ERR_USER_2);

    return gerr;
}
