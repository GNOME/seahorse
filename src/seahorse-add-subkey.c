/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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
 
#include "seahorse-gpgmex.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-key-op.h"
#include "seahorse-util.h"

#define EXPIRES "expires"
#define LENGTH "length"

static void
type_changed (GtkOptionMenu *optionmenu, SeahorseWidget *swidget)
{
	gint type;
	GtkSpinButton *length;
	
	length = GTK_SPIN_BUTTON (glade_xml_get_widget (swidget->xml, LENGTH));
	type = gtk_option_menu_get_history (optionmenu);
	
	switch (type) {
		/* DSA */
		case 0:
			gtk_spin_button_set_range (length, DSA_MIN, DSA_MAX);
			break;
		/* ElGamal */
		case 1:
			gtk_spin_button_set_range (length, ELGAMAL_MIN, LENGTH_MAX);
			break;
		/* RSA */
		default:
			gtk_spin_button_set_range (length, RSA_MIN, LENGTH_MAX);
			break;
	}
}

static void
never_expires_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, EXPIRES),
		!gtk_toggle_button_get_active (togglebutton));
}

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	SeahorseKeyEncType real_type;
	guint type, length;
	time_t expires;
	gpgme_error_t err;
	GtkWidget *widget;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	type = gtk_option_menu_get_history (GTK_OPTION_MENU (
		glade_xml_get_widget (swidget->xml, "type")));
	length = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (
		glade_xml_get_widget (swidget->xml, LENGTH)));
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	glade_xml_get_widget (swidget->xml, "never_expires"))))
		expires = 0;
	else
		expires = gnome_date_edit_get_time (GNOME_DATE_EDIT (
			glade_xml_get_widget (swidget->xml, EXPIRES)));
	
	switch (type) {
		case 0:
			real_type = DSA;
			break;
		case 1:
			real_type = ELGAMAL;
			break;
		case 2:
			real_type = RSA_SIGN;
			break;
		default:
			real_type = RSA_ENCRYPT;
			break;
	}
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_widget_set_sensitive (widget, FALSE);
	err = seahorse_key_pair_op_add_subkey (SEAHORSE_PGP_KEY (skwidget->skey), 
                                           real_type, length, expires);
	gtk_widget_set_sensitive (widget, TRUE);
	
	if (!GPG_IS_OK (err))
		seahorse_util_handle_gpgme (err, _("Couldn't add subkey"));
	else
		seahorse_widget_destroy (swidget);
}

void
seahorse_add_subkey_new (SeahorsePGPKey *pkey)
{
	SeahorseWidget *swidget;
    gchar *userid;
	
	swidget = seahorse_key_widget_new ("add-subkey", SEAHORSE_KEY (pkey));
	g_return_if_fail (swidget != NULL);
	
    userid = seahorse_key_get_name (SEAHORSE_KEY (pkey), 0);
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
		g_strdup_printf (_("Add subkey to %s"), userid));
    g_free (userid);
	
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "never_expires_toggled",
		G_CALLBACK (never_expires_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "type_changed",
		G_CALLBACK (type_changed), swidget);
}
