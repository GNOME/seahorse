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

#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"

GtkWindow* 
seahorse_signatures_new (SeahorseContext *sctx, gpgme_verify_result_t status)
{
	SeahorseWidget *swidget;
	gpgme_key_t key;
	SeahorseKey *skey;
	const gchar *label;
    gchar *userid;
    gboolean good = FALSE;
    	
	swidget = seahorse_widget_new_allow_multiple ("signatures", sctx);
    
    if (status->signatures) {
    	switch (gpgme_err_code (status->signatures->status)) {
    		case GPG_ERR_KEY_EXPIRED:
    			label = _("Good, signing key expired");
                good = TRUE;
    			break;
    		case GPG_ERR_SIG_EXPIRED:
    			label = _("Good, signature expired");
                good = TRUE;
    			break;
    		case GPG_ERR_CERT_REVOKED:
    			label = _("Good, signing key certificate revoked");
                good = TRUE;
    			break;
    		case GPG_ERR_NO_ERROR:
    			label = _("Good");
                good = TRUE;
    			break;
            case GPG_ERR_NO_PUBKEY:
                label = _("Signing key not in key ring");
                break;
            case GPG_ERR_BAD_SIGNATURE:
                label = _("Bad Signature");
                break;
            case GPG_ERR_NO_DATA:
               label =   _("Not a signature");
               break;
            default:
               label = _("Verification Error");
               break;
    	}
    } else {
        label = _("No signature");
    }

    gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "status")), label);

    if(good)
    {				
		gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "created")),
			seahorse_util_get_date_string (status->signatures->timestamp));

        gpgme_get_key (sctx->ctx, status->signatures->fpr, &key, 0);
        skey = seahorse_context_get_key (sctx, key);
		userid = seahorse_key_get_userid (skey, 0);
		gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "signer")),
			userid);
		g_free (userid);
		gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "keyid")),
			seahorse_key_get_keyid (skey, 0));
		gpgme_key_unref (key);
    }          

    return GTK_WINDOW(glade_xml_get_widget(swidget->xml, "signatures"));
}
