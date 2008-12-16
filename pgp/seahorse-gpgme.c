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

GQuark
seahorse_gpgme_error_domain (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("seahorse-gpgme-error");
	return q;
}

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

static gpgme_key_t
ref_return_key (gpgme_key_t key)
{
	gpgme_key_ref (key);
	return key;
}


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
