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
#include <time.h>
#include <gpgme.h>

#include "seahorse-key-properties.h"
#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-export.h"
#include "seahorse-delete.h"
#include "seahorse-validity.h"

#define SPACING 12
#define TRUST "trust"
#define EXPIRES "expires"
#define NEVER_EXPIRES "never_expires"
#define DISABLED "disabled"

/* Loads export dialog */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	seahorse_export_new (swidget->sctx, skwidget->skey);
}

/* Loads delete dialog */
static void
delete_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	seahorse_delete_new (NULL,
		swidget->sctx, skwidget->skey);
}

/* Tries to change the key's owner trust */
static void
trust_changed (GtkOptionMenu *optionmenu, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	guint history;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	history = gtk_option_menu_get_history (optionmenu);
	seahorse_ops_key_set_trust (swidget->sctx, skwidget->skey, seahorse_validity_get_from_index (history));
}

/* Tries to change the key's primary expiration date */
static void
expires_date_changed (GnomeDateEdit *gde, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	time_t expires;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	expires = gnome_date_edit_get_time (gde);
	
	/* Cannot set expiration to before NOW */
	if (expires <= time (NULL)) {
		g_signal_handlers_disconnect_by_func (gde, expires_date_changed, swidget);
		gnome_date_edit_set_time (gde, time (NULL));
		g_signal_connect_after (gde, "date_changed", G_CALLBACK (expires_date_changed), swidget);
	}
	else
		seahorse_ops_key_set_expires (swidget->sctx, skwidget->skey, expires);
}

static void
toggle_expires (GtkWidget *widget, gboolean never_expires)
{
	if (GNOME_IS_DATE_EDIT (widget)) {
		gtk_widget_set_sensitive (widget, !never_expires);
		
		if (never_expires)
			gnome_date_edit_set_time (GNOME_DATE_EDIT (widget), time (NULL));
	}
}

/* Tries to change whether the key expires */
static void
never_expires_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	gboolean active;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	active = gtk_toggle_button_get_active (togglebutton);
	
	if (!active || (active && seahorse_ops_key_set_expires (swidget->sctx, skwidget->skey, 0)))
		gtk_container_foreach (GTK_CONTAINER (glade_xml_get_widget (swidget->xml, "table")),
			(GtkCallback)toggle_expires, (gpointer)active);
}

/* Changes disabled state of key */
static void
disabled_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	seahorse_ops_key_set_disabled (swidget->sctx, skwidget->skey,
		gtk_toggle_button_get_active (togglebutton));
}

/* Changes key's passphrase */
static void
change_passphrase_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	
	/* Assume key has secret since item shouldn't be visible otherwise */
	seahorse_ops_key_change_pass (swidget->sctx, skwidget->skey);
}

/* Do a label */
static void
do_stat_label (const gchar *label, GtkTable *table, guint left, guint top)
{
	GtkWidget *widget;
	GtkWidget *align;
	
	widget = gtk_label_new (label);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	align = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER (align), widget);
	gtk_table_attach (table, align, left, left+1, top, top+1, GTK_FILL, 0, 0, 0);
	gtk_widget_show_all (align);
}

/* Do statistics common to primary and sub keys */
static void
do_stats (GtkTable *table, guint top, SeahorseKey *skey,
	  guint index, gboolean has_secret, SeahorseWidget *swidget)
{
	time_t expires;
	GtkWidget *widget;
	
	do_stat_label (_("Key ID:"), table, 0, top);
	do_stat_label (seahorse_key_get_keyid (skey, index), table, 1, top);
	do_stat_label (_("Algorithm:"), table, 2, top);
	do_stat_label (gpgme_key_get_string_attr (skey->key, GPGME_ATTR_ALGO, NULL, index), table, 3, top);
	do_stat_label (_("Created:"), table, 0, top+1);
	do_stat_label (seahorse_util_get_date_string (gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_CREATED, NULL, index)),
		table, 1, top+1);
	do_stat_label (_("Length:"), table, 2, top+1);
	do_stat_label (g_strdup_printf ("%d", gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_LEN, NULL, index)),
		table, 3, top+1);
	do_stat_label (_("Expires:"), table, 0, top+2);
	
	expires = gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_EXPIRE, NULL, index);
	
	if (!has_secret && expires)
		do_stat_label (seahorse_util_get_date_string (expires), table, 1, top+2);
	else if (!has_secret && !expires)
		do_stat_label (_("Never Expires"), table, 1, top+2);
	else {
		widget = gnome_date_edit_new (expires, FALSE, FALSE);
		gtk_widget_show (widget);
		gtk_table_attach (table, widget, 1, 3, top+2, top+3, GTK_FILL, 0, 0, 0);
		
		if (index == 0) {
			gtk_widget_set_sensitive (widget, expires);
			g_signal_connect_after (widget, "date_changed", G_CALLBACK (expires_date_changed), swidget);
		}
		else
			gtk_widget_set_sensitive (widget, FALSE);
		
		widget = gtk_check_button_new_with_mnemonic (_("_Never Expires"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !expires);
		gtk_widget_show (widget);
		gtk_table_attach (table, widget, 3, 4, top+2, top+3, GTK_FILL, 0, 0, 0);
		
		if (index == 0)
			g_signal_connect_after (widget, "toggled", G_CALLBACK (never_expires_toggled), swidget);
		else
			gtk_widget_set_sensitive (widget, FALSE);
	}
}

void
seahorse_key_properties_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseWidget *swidget;
	GtkWidget *widget;
	GtkOptionMenu *option;
	GtkNotebook *notebook;
	GtkWidget *label;
	GtkTable *table;
        time_t expires_date;
        GtkContainer *vbox;
        GSList *primary_group = NULL;
	gint index = 0;
	gboolean has_secret;
	
	swidget = seahorse_key_widget_new_component ("key-properties", sctx, skey);
	g_return_if_fail (swidget != NULL);
	
	/* Hide edit menu if key doesn't have secret component */
	if (!seahorse_context_key_has_secret (sctx, skey)) {
		widget = glade_xml_get_widget (swidget->xml, "edit_menu");
		gtk_widget_hide (widget);
	}
	
	/* Primary key attributes */
	widget = glade_xml_get_widget (swidget->xml, "fingerprint");
	gtk_label_set_text (GTK_LABEL (widget),
		gpgme_key_get_string_attr (skey->key, GPGME_ATTR_FPR, NULL, 0));
		
	option = GTK_OPTION_MENU (glade_xml_get_widget (swidget->xml, TRUST));
	seahorse_validity_load_menu (option);
	gtk_option_menu_set_history (option, seahorse_validity_get_index (
		gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_OTRUST, NULL, 0)));
		
	widget = glade_xml_get_widget (swidget->xml, DISABLED);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
		gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_KEY_DISABLED, NULL, 0));
	
	has_secret = seahorse_context_key_has_secret (sctx, skey);
	
	do_stats (GTK_TABLE (glade_xml_get_widget (swidget->xml, "table")), 2, skey, 0, has_secret, swidget);
	
	/* User ids */
	notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	vbox = GTK_CONTAINER (glade_xml_get_widget (swidget->xml, "vbox"));
	gtk_container_add (vbox, GTK_WIDGET (notebook));
	
	while (index < seahorse_key_get_num_uids (skey)) {
		
		label = gtk_label_new (g_strdup_printf (_("User ID %d"), (index+1)));
		
		if (has_secret) {
			table = GTK_TABLE (gtk_table_new (2, 2, FALSE));
			
			widget = gtk_radio_button_new_with_mnemonic (primary_group, _("Primary"));
			primary_group = g_slist_append (primary_group, widget);
			//callback
			gtk_widget_set_sensitive (widget, FALSE);
			gtk_table_attach (table, widget, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
		
			widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
			//callback
			gtk_widget_set_sensitive (widget, FALSE);
			gtk_table_attach (table, widget, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);
		}
		else
			table = GTK_TABLE (gtk_table_new (1, 2, FALSE));
		
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		
		gtk_notebook_append_page (notebook, GTK_WIDGET (table), label);
		
		widget = gtk_label_new (seahorse_key_get_userid (skey, index));
		gtk_table_attach (table, widget, 0, 3, 0, 1, GTK_FILL, 0, 0, 0);
		
		index++;
	}
	gtk_widget_show_all (GTK_WIDGET (notebook));
	
	/* Sub keys */
	notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_container_add (vbox, GTK_WIDGET (notebook));
	
	index = 1;
	while (index <= seahorse_key_get_num_subkeys (skey)) {
		
		label = gtk_label_new (g_strdup_printf (_("Sub Key %d"), index));
		
		if (has_secret) {
			table = GTK_TABLE (gtk_table_new (4, 4, FALSE));
			
			widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
			//set callback
			gtk_widget_set_sensitive (widget, FALSE);
			gtk_widget_show (widget);
			gtk_table_attach (table, widget, 3, 4, 3, 4, GTK_FILL, 0, 0, 0);
		}
		else
			table = GTK_TABLE (gtk_table_new (3, 4, FALSE));
		
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		
		gtk_notebook_append_page (notebook, GTK_WIDGET (table), label);
		gtk_widget_show_all (GTK_WIDGET (notebook));
		
		do_stats (table, 0, skey, index, has_secret, swidget);
		
		index++;
	}
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_window_set_title (GTK_WINDOW (widget), seahorse_key_get_userid (skey, 0));
	
	/* Have to connect some signals after so changes don't activate callbacks */
	glade_xml_signal_connect_data (swidget->xml, "export_activate", G_CALLBACK (export_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "delete_activate", G_CALLBACK (delete_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "trust_changed", G_CALLBACK (trust_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "disabled_toggled", G_CALLBACK (disabled_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "change_passphrase_activate", G_CALLBACK (change_passphrase_activate), swidget);
}
