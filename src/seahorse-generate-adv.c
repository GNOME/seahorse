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

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-key-op.h"

#define LENGTH "length"
#define EXPIRES "expires"
#define NAME "name"
#define PASS "passphrase"
#define CONFIRM "confirm"

static void
entry_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	gchar *name, *pass, *confirm;
	
	name = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (
		swidget->xml, NAME)), 0, -1);
	pass = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (
		swidget->xml, PASS)), 0, -1);
	confirm = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (
		swidget->xml, CONFIRM)), 0, -1);
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "ok"),
		(strlen (name) >= 5 && strlen (pass) > 0 && g_str_equal (pass, confirm)));
}

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
	const gchar *name, *email, *comment, *pass;
	gint history, length;
	SeahorseKeyType type;
	time_t expires;
	GtkWidget *widget;
	gpgme_error_t err;
	
	name = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, NAME)));
	email = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, "email")));
	comment = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, "comment")));
	pass = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, PASS)));
	
	length = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (
		glade_xml_get_widget (swidget->xml, LENGTH)));
	history = gtk_option_menu_get_history (GTK_OPTION_MENU (
		glade_xml_get_widget (swidget->xml, "type")));
	
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
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	glade_xml_get_widget (swidget->xml, "never_expires"))))
		expires = 0;
	else
		expires = gnome_date_edit_get_time (GNOME_DATE_EDIT (
			glade_xml_get_widget (swidget->xml, EXPIRES)));
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_widget_hide (widget);
	
	err = seahorse_key_op_generate (swidget->sctx, name, email, comment,
		pass, type, length, expires);
	if (!GPG_IS_OK (err)) {
		gtk_widget_show (widget);
		seahorse_util_handle_error (err, _("Couldn't generate key"));
	}
	else
		seahorse_widget_destroy (swidget);
}

void
seahorse_generate_adv_show (SeahorseContext *sctx)
{	
	SeahorseWidget *swidget;
	
	swidget = seahorse_widget_new ("generate-adv", sctx);
	g_return_if_fail (swidget != NULL);
	
	glade_xml_signal_connect_data (swidget->xml, "type_changed",
		G_CALLBACK (type_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "never_expires_toggled",
		G_CALLBACK (never_expires_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "entry_changed",
		G_CALLBACK (entry_changed), swidget);
}
