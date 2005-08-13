/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Nate Nielsen
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

#include <stdlib.h>
#include <libintl.h>
#include <gnome.h>

#include "seahorse-operation.h"
#include "seahorse-progress.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-validity.h"
#include "seahorse-recipients-store.h"
#include "seahorse-default-key-control.h"
#include "seahorse-gconf.h"
#include "seahorse-pgp-key.h"

SeahorsePGPKey*
seahorse_signer_get ()
{
	SeahorseWidget *swidget;
    SeahorseKeyset *skset;
    SeahorseDefaultKeyControl *sdkc;
    SeahorseKey *skey = NULL;
	GtkWidget *widget;
	gint response;
	gboolean done = FALSE;
    gboolean ok = FALSE;
    gchar *id;
    
    skey = seahorse_context_get_default_key (SCTX_APP ());
    if (skey != NULL && SEAHORSE_IS_PGP_KEY (skey))
        return SEAHORSE_PGP_KEY (skey);
	
	swidget = seahorse_widget_new ("signer");
	g_return_val_if_fail (swidget != NULL, NULL);
	        
    widget = glade_xml_get_widget (swidget->xml, "sign_key_place");

    skset = seahorse_keyset_new (SKEY_PGP, SKEY_PRIVATE, SKEY_LOC_LOCAL, SKEY_FLAG_CAN_SIGN);
    sdkc = seahorse_default_key_control_new (skset, NULL);
    g_object_unref (skset);
    
    gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (sdkc));
    gtk_widget_show_all (widget);    

    /* Select the last key used */
    id = seahorse_gconf_get_string (LASTSIGNER_KEY);
    seahorse_default_key_control_select_id (sdkc, id);
    g_free (id); 
    
	widget = seahorse_widget_get_top (swidget);
    seahorse_widget_show (swidget);
    
	while (!done) {
		response = gtk_dialog_run (GTK_DIALOG (widget));
		switch (response) {
			case GTK_RESPONSE_HELP:
				break;
			case GTK_RESPONSE_OK:
				ok = TRUE;
			default:
				done = TRUE;
				break;
		}
	}

    if (ok) {
        skey = seahorse_default_key_control_active (sdkc);
        g_return_val_if_fail (SEAHORSE_IS_PGP_KEY (skey), NULL);

        /* Save this as the last key signed with */
        seahorse_gconf_set_string (LASTSIGNER_KEY, skey == NULL ? 
                        "" : seahorse_key_get_keyid (skey));
    }
    
	seahorse_widget_destroy (swidget);
	return SEAHORSE_PGP_KEY (skey);
}
