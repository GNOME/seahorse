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

#include "seahorse-generate.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-ops-key.h"

#define LENGTH "length"
#define EXPIRES "expires"

/* Sets range of length spin button */
static void
type_changed (GtkOptionMenu *optionmenu, SeahorseWidget *swidget)
{
	gint type;
	GtkSpinButton *length;
	
	type = gtk_option_menu_get_history (optionmenu);
	length = GTK_SPIN_BUTTON (glade_xml_get_widget (swidget->xml, LENGTH));
	
	switch (type) {
		/* DSA_ELGAMAL */
		case 0:
			gtk_spin_button_set_range (length, ELGAMAL_MIN, LENGTH_MAX);
			break;
		/* DSA */
		case 1:
			gtk_spin_button_set_range (length, DSA_MIN, DSA_MAX);
			break;
		/* RSA_SIGN */
		default:
			gtk_spin_button_set_range (length, RSA_MIN, LENGTH_MAX);
			break;
	}
}

/* Toggles expiration date sensitivity */
static void
never_expires_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, EXPIRES),
		!gtk_toggle_button_get_active (togglebutton));
}

/* Generates a key */
static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	const gchar *name, *email, *comment, *passphrase, *confirm;
	gint history, length;
	SeahorseKeyType type;
	time_t expires;
	GtkWindow *parent;
	
	name = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, "name")));
	email = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, "email")));
	comment = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, "comment")));
	passphrase = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, "passphrase")));
	confirm = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, "confirm")));
	
	parent = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));
	
	/* Check for valid entries */
	if (g_str_equal (name, "")) {
		seahorse_util_show_error (parent, _("You must fill in a valid name"));
		return;
	}	
	if (!seahorse_ops_key_check_email (email)) {
		seahorse_util_show_error (parent, _("You must fill in a valid email address"));
		return;
	}	
	if (g_str_equal (comment, "")) {
		seahorse_util_show_error (parent, _("You must fill in a valid comment"));
		return;
	}	
	if (g_str_equal (passphrase, "")) {
		seahorse_util_show_error (parent, _("You must fill in a valid passphrase"));
		return;
	}	
	if (g_str_equal (confirm, "")) {
		seahorse_util_show_error (parent, _("You must confirm your passphrase"));
		return;
	}
	if (!g_str_equal (passphrase, confirm)) {
		seahorse_util_show_error (parent, _("Passphrase & confirmation must match"));
		return;
	}
	
	length = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (
		glade_xml_get_widget (swidget->xml, LENGTH)));
	history = gtk_option_menu_get_history (GTK_OPTION_MENU (glade_xml_get_widget (swidget->xml, "type")));
	
	switch (history) {
		case 0:
			type = DSA_ELGAMAL;
			break;
		case 1:
			type = DSA;
			break;
		default:
			type = RSA_SIGN;
			break;
	}
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, "never_expires"))))
		expires = 0;
	else
		expires = gnome_date_edit_get_time (GNOME_DATE_EDIT (glade_xml_get_widget (swidget->xml, EXPIRES)));
	
	gtk_widget_hide (GTK_WIDGET (parent));
	seahorse_ops_key_generate (swidget->sctx, name, email, comment, passphrase, type, length, expires);
	gtk_widget_show (GTK_WIDGET (parent));
	seahorse_widget_destroy (swidget);
}

void
seahorse_generate_show (SeahorseContext *sctx)
{	
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("generate", sctx);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "type_changed", G_CALLBACK (type_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "never_expires_toggled", G_CALLBACK (never_expires_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked", G_CALLBACK (ok_clicked), swidget);
}
