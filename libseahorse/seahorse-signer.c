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
#include <eel/eel.h>

#include "seahorse-operation.h"
#include "seahorse-progress.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-validity.h"
#include "seahorse-recipients-store.h"
#include "seahorse-default-key-control.h"

SeahorseKeyPair*
seahorse_signer_get (SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
    SeahorseKeySource *sksrc;
    SeahorseDefaultKeyControl *sdkc;
    SeahorseKeyPair *signer = NULL;
	GtkWidget *widget;
	gint response;
	gboolean done = FALSE;
    gboolean ok = FALSE;
    gchar *id;
    
    if (eel_gconf_get_boolean (SIGNDEFAULT_KEY))
        return seahorse_context_get_default_key (sctx);
	
	swidget = seahorse_widget_new ("signer", sctx);
	g_return_val_if_fail (swidget != NULL, NULL);
	        
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, NULL);

    widget = glade_xml_get_widget (swidget->xml, "sign_key_place");
    sdkc = seahorse_default_key_control_new (sksrc, NULL);
    gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (sdkc));
    gtk_widget_show_all (widget);    

    /* Select the last key used */
    id = eel_gconf_get_string (LASTSIGNER_KEY);
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
        signer = seahorse_default_key_control_active (sdkc);

        /* Save this as the last key signed with */
        eel_gconf_set_string (LASTSIGNER_KEY, signer == NULL ? 
                "" : seahorse_key_pair_get_id (signer));
    }
    
	seahorse_widget_destroy (swidget);
	return signer;
}
