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

static void
sig_error (GpgmeSigStat status)
{
	gchar *label;
	GtkWidget *widget;
	
	switch (status) {
		case GPGME_SIG_STAT_NOKEY:
			label = _("Signing key not in key ring");
			break;
		/* Bad sig */
		case GPGME_SIG_STAT_BAD:
			label = _("Bad Signature");
			break;
		/* Not a signature */
		case GPGME_SIG_STAT_NOSIG:
			label =   _("Not a signature");
			break;
		/* Other */
		default:
			label = _("Verification Error");
			break;
	}
	
	widget = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, label);
	gtk_dialog_run (GTK_DIALOG (widget));
	gtk_widget_destroy (widget);
}

void
seahorse_signatures_new (SeahorseContext *sctx, GpgmeSigStat status)
{
	SeahorseWidget *swidget;
	GpgmeKey key;
	SeahorseKey *skey;
	gchar *label;
	
	switch (status) {
		/* If sig is good or there are multiple sigs, show */
		case GPGME_SIG_STAT_GOOD: GPGME_SIG_STAT_GOOD_EXPKEY: GPGME_SIG_STAT_DIFF:	
			swidget = seahorse_widget_new_allow_multiple ("signatures", sctx);
			gpgme_get_sig_key (sctx->ctx, 0, &key);
			skey = seahorse_context_get_key (sctx, key);
				
			switch (status) {
				case GPGME_SIG_STAT_GOOD_EXPKEY:
					label = _("Good, signing key expired");
					break;
				case GPGME_SIG_STAT_DIFF:
					label = _("Multiple Signatures");
					break;
				default:
					label = _("Good");
					break;
			}
				
			gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "status")), label);
			gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "created")),
				seahorse_util_get_date_string (gpgme_get_sig_ulong_attr (sctx->ctx, 0, GPGME_ATTR_CREATED, 0)));
			gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "signer")),
				seahorse_key_get_userid (skey, 0));
			gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "keyid")),
				seahorse_key_get_keyid (skey, 0));
			break;
		default:
			sig_error (status);
			break;
	}
}
