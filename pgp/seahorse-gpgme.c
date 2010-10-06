/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "config.h"

#include "seahorse-gpgme.h"

#include "seahorse-util.h"

#include <glib/gi18n.h>

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
GQuark
seahorse_gpgme_error_domain (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("seahorse-gpgme-error");
	return q;
}

/**
 * seahorse_gpgme_to_error:
 * @gerr: The gpgme error
 * @err: The glib error to create
 *
 * Creates a glib error out of a gpgme error
 *
 */
void    
seahorse_gpgme_to_error (gpgme_error_t gerr, GError** err)
{
	gpgme_err_code_t code;
    
    	/* Make sure this is actually an error */
    	code = gpgme_err_code (gerr);
   	g_return_if_fail (code != 0);
    
    	/* Special case some error messages */
    	if (code == GPG_ERR_DECRYPT_FAILED) {
    		g_set_error (err, SEAHORSE_GPGME_ERROR, code, "%s", 
    		             _("Decryption failed. You probably do not have the decryption key."));
    	} else {
    		g_set_error (err, SEAHORSE_GPGME_ERROR, code, "%s", 
    		             gpgme_strerror (gerr));
    	}
}

/**
 * seahorse_gpgme_handle_error:
 * @err: the gpgme error to handle (display)
 * @desc: a printf formated string
 * @...: varargs to fill into this string
 *
 * Creates the heading of an error out of desc and the varargs. Displays the error
 *
 * The content of the error box is supported by gpgme (#gpgme_strerror)
 *
 * Some errors are not displayed.
 *
 */
void
seahorse_gpgme_handle_error (gpgme_error_t err, const char* desc, ...)
{
	gchar *t = NULL;

	switch (gpgme_err_code(err)) {
	case GPG_ERR_CANCELED:
	case GPG_ERR_NO_ERROR:
	case GPG_ERR_ECANCELED:
		return;
	default: 
		break;
	}

	va_list ap;
	va_start(ap, desc);
  
	if (desc) 
		t = g_strdup_vprintf (desc, ap);

	va_end(ap);
        
	seahorse_util_show_error (NULL, t, gpgme_strerror (err));
	g_free(t);
}

/**
 * ref_return_key:
 * @key: the gpgme key
 *
 * Acquires an additional gpgme reference for the key
 *
 * Returns: the key
 */
static gpgme_key_t
ref_return_key (gpgme_key_t key)
{
	gpgme_key_ref (key);
	return key;
}

/**
 * seahorse_gpgme_boxed_key_type:
 *
 * Creates a new boxed type "gpgme_key_t"
 *
 * Returns: the new boxed type
 */
GType
seahorse_gpgme_boxed_key_type (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("gpgme_key_t", 
		                                     (GBoxedCopyFunc)ref_return_key,
		                                     (GBoxedFreeFunc)gpgme_key_unref);
	return type;
}

/**
 * seahorse_gpgme_convert_validity:
 * @validity: the gpgme validity of a key
 *
 * converts the gpgme validity to the seahorse validity
 *
 * Returns: The seahorse validity
 */
SeahorseValidity
seahorse_gpgme_convert_validity (gpgme_validity_t validity)
{
	switch (validity) {
	case GPGME_VALIDITY_NEVER:
		return SEAHORSE_VALIDITY_NEVER;
	case GPGME_VALIDITY_MARGINAL:
		return SEAHORSE_VALIDITY_MARGINAL;
	case GPGME_VALIDITY_FULL:
		return SEAHORSE_VALIDITY_FULL;
	case GPGME_VALIDITY_ULTIMATE:
		return SEAHORSE_VALIDITY_ULTIMATE;
	case GPGME_VALIDITY_UNDEFINED:
	case GPGME_VALIDITY_UNKNOWN:
	default:
		return SEAHORSE_VALIDITY_UNKNOWN;
	}	
}

/*
 * Based on the values in ask_algo() in gnupg's g10/keygen.c
 * http://cvs.gnupg.org/cgi-bin/viewcvs.cgi/trunk/g10/keygen.c?rev=HEAD&root=GnuPG&view=log
 */
const struct _SeahorseKeyTypeTable KEYTYPES_2012 =
	{ .rsa_sign=4, .rsa_enc=6, .dsa_sign=3, .elgamal_enc=5 };
const struct _SeahorseKeyTypeTable KEYTYPES_140 =
	{ .rsa_sign=5, .rsa_enc=6, .dsa_sign=2, .elgamal_enc=4 };
const struct _SeahorseKeyTypeTable KEYTYPES_124 =
	{ .rsa_sign=4, .rsa_enc=5, .dsa_sign=2, .elgamal_enc=3 };
const struct _SeahorseKeyTypeTable KEYTYPES_120 =
	{ .rsa_sign=5, .rsa_enc=6, .dsa_sign=2, .elgamal_enc=3 };

const SeahorseVersion VER_2012 = seahorse_util_version(2,0,12,0);
const SeahorseVersion VER_190  = seahorse_util_version(1,9,0,0);
const SeahorseVersion VER_1410 = seahorse_util_version(1,4,10,0);
const SeahorseVersion VER_140  = seahorse_util_version(1,4,0,0);
const SeahorseVersion VER_124  = seahorse_util_version(1,2,4,0);
const SeahorseVersion VER_120  = seahorse_util_version(1,2,0,0);

/**
 * seahorse_gpgme_get_keytype_table:
 *
 * @table: The requested keytype table
 *
 * Based on the gpg version in use, sets @table
 * to contain the numbers that gpg uses in its CLI
 * for adding new subkeys. This tends to get broken
 * at random by new versions of gpg, but there's no good
 * API for this.
 *
 * Returns GPG_ERR_USER_2 if gpg is too old.
 *
 * Returns: gpgme_error_t
 **/
gpgme_error_t
seahorse_gpgme_get_keytype_table (SeahorseKeyTypeTable *table)
{
	gpgme_error_t gerr;
	gpgme_engine_info_t engine;
	SeahorseVersion ver;
	
	gerr = gpgme_get_engine_info (&engine);
	g_return_val_if_fail (GPG_IS_OK (gerr), gerr);
	
	while (engine && engine->protocol != GPGME_PROTOCOL_OpenPGP)
		engine = engine->next;
	g_return_val_if_fail (engine != NULL, GPG_E (GPG_ERR_GENERAL));
	
	ver = seahorse_util_parse_version (engine->version);
	
	if (ver >= VER_2012 || (ver >= VER_1410 && ver < VER_190))
		*table = (SeahorseKeyTypeTable)&KEYTYPES_2012;
	else if (ver >= VER_140 || ver >= VER_190)
		*table = (SeahorseKeyTypeTable)&KEYTYPES_140;
	else if (ver >= VER_124)
		*table = (SeahorseKeyTypeTable)&KEYTYPES_124;
	else if (ver >= VER_120)
		*table = (SeahorseKeyTypeTable)&KEYTYPES_120;
	else	// older versions not supported
		gerr = GPG_E (GPG_ERR_USER_2);
	
	return gerr;
}

