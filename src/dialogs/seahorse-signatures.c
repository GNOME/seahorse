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

#include "seahorse-signatures.h"
#include "seahorse-util.h"
#include "seahorse-key-properties.h"
#include "seahorse-key-widget.h"

/* Load key's properties */
static void
view_key_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	seahorse_key_properties_new (swidget->sctx, skwidget->skey);
}

void
seahorse_signatures_new (SeahorseContext *sctx, GpgmeSigStat status, gboolean show_key)
{
	SeahorseWidget *swidget;
	GpgmeKey key;
	gchar *label;
	
	switch (status) {
		/* If sig is good or there are multiple sigs, show */
		case GPGME_SIG_STAT_GOOD: GPGME_SIG_STAT_GOOD_EXPKEY: GPGME_SIG_STAT_DIFF:
			if (gpgme_get_sig_key (sctx->ctx, 0, &key) == GPGME_No_Error) {
				swidget = seahorse_key_widget_new ("signatures", sctx, seahorse_context_get_key (sctx, key));
				
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
				
				if (show_key) {
					gtk_widget_show (glade_xml_get_widget (swidget->xml, "view_key"));
					glade_xml_signal_connect_data (swidget->xml, "view_key_clicked",
						G_CALLBACK (view_key_clicked), swidget);
				}
			}
			else
				seahorse_util_show_error (NULL, _("Cannot get signing key."));
			break;
		/* Don't have key */
		case GPGME_SIG_STAT_NOKEY:
			seahorse_util_show_error (NULL, _("Signing key not in key ring"));
			break;
		/* Bad sig */
		case GPGME_SIG_STAT_BAD:
			seahorse_util_show_error (NULL, _("Bad Signature"));
			break;
		/* Not a signature */
		case GPGME_SIG_STAT_NOSIG:
			seahorse_util_show_error (NULL, _("Not a signature"));
			break;
		/* Other */
		default:
			seahorse_util_show_error (NULL, _("Verification Error"));
			break;
	}
}
