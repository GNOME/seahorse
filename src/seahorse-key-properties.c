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
#include <time.h>
#include <gpgme.h>

#include "seahorse-key-properties.h"
#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-export.h"
#include "seahorse-delete.h"
#include "seahorse-validity.h"
#include "seahorse-add-uid.h"
#include "seahorse-add-subkey.h"
#include "seahorse-revoke.h"

#define SPACING 12
#define TRUST "trust"
#define EXPIRES "expires"
#define NEVER_EXPIRES "never_expires"
#define DISABLED "disabled"
#define SUBKEYS "subkeys"

/* Loads export dialog */
static void
export_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_export_new (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey);
}

/* Loads delete dialog */
static void
delete_activate (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_delete_key_new (NULL, swidget->sctx,
		SEAHORSE_KEY_WIDGET (swidget)->skey);
}

/* Tries to change the key's owner trust */
static void
trust_changed (GtkOptionMenu *optionmenu, SeahorseWidget *swidget)
{
	seahorse_ops_key_set_trust (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		gtk_option_menu_get_history (optionmenu) + 1);
}

/* Primary key's never expires toggled */
static void
never_expires_primary_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	if (gtk_toggle_button_get_active (togglebutton)) {
		seahorse_ops_key_set_expires (swidget->sctx,
			SEAHORSE_KEY_WIDGET (swidget)->skey, 0, 0);
	}
}

/* Subkey's never expires toggled */
static void
never_expires_subkey_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{	
	if (gtk_toggle_button_get_active (togglebutton)) {
		gint index;
		
		index = gtk_notebook_get_current_page (GTK_NOTEBOOK (
			glade_xml_get_widget (swidget->xml, SUBKEYS))) + 1;
		seahorse_ops_key_set_expires (swidget->sctx,
			SEAHORSE_KEY_WIDGET (swidget)->skey, index, 0);
	}
}

static void
primary_expires_date_changed (GnomeDateEdit *gde, SeahorseWidget *swidget)
{
	seahorse_ops_key_set_expires (swidget->sctx,
		SEAHORSE_KEY_WIDGET (swidget)->skey, 0,
		gnome_date_edit_get_time (gde));
}

static void
subkey_expires_date_changed (GnomeDateEdit *gde, SeahorseWidget *swidget)
{
	gint index;
	
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (
		glade_xml_get_widget (swidget->xml, SUBKEYS))) + 1;
	seahorse_ops_key_set_expires (swidget->sctx,
		SEAHORSE_KEY_WIDGET (swidget)->skey, index,
		gnome_date_edit_get_time (gde));
}

static void
set_date_edit_sensitive (GtkToggleButton *togglebutton, GtkWidget *widget)
{
	gtk_widget_set_sensitive (widget, !gtk_toggle_button_get_active (togglebutton));
}

/* Changes disabled state of key */
static void
disabled_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	seahorse_ops_key_set_disabled (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		gtk_toggle_button_get_active (togglebutton));
}

/* Changes key's passphrase */
static void
change_passphrase_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_ops_key_change_pass (swidget->sctx,
		SEAHORSE_KEY_WIDGET (swidget)->skey);
}

static void
add_uid_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_add_uid_new (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey);
}

static void
add_subkey_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	seahorse_add_subkey_new (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey);
}

static void
subkeys_activate (GtkMenuItem *item, SeahorseWidget *swidget)
{
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
		gtk_widget_show (glade_xml_get_widget (swidget->xml, SUBKEYS));
	else
		gtk_widget_hide (glade_xml_get_widget (swidget->xml, SUBKEYS));
}

static void
del_subkey_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	guint index;
	
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (
		glade_xml_get_widget (swidget->xml, SUBKEYS))) + 1;
	
	seahorse_delete_subkey_new (GTK_WINDOW (glade_xml_get_widget (
		swidget->xml, swidget->name)), swidget->sctx,
		SEAHORSE_KEY_WIDGET (swidget)->skey, index);
}

static void
revoke_subkey_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	guint index;
	
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (
		glade_xml_get_widget (swidget->xml, SUBKEYS))) + 1;
	seahorse_revoke_subkey_new (swidget->sctx,
		SEAHORSE_KEY_WIDGET (swidget)->skey, index);
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
do_stats (SeahorseWidget *swidget, GtkTable *table, guint top, guint index)
{
	SeahorseKey *skey;
	time_t expires;
	GtkWidget *widget, *date_edit;
	GtkTooltips *tooltips;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	tooltips = gtk_tooltips_new ();
	
	do_stat_label (_("Key ID:"), table, 0, top);
	do_stat_label (seahorse_key_get_keyid (skey, index), table, 1, top);
	do_stat_label (_("Type:"), table, 2, top);
	do_stat_label (gpgme_key_get_string_attr (skey->key, GPGME_ATTR_ALGO, NULL, index), table, 3, top);
	do_stat_label (_("Created:"), table, 0, top+1);
	do_stat_label (seahorse_util_get_date_string (gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_CREATED, NULL, index)),
		table, 1, top+1);
	do_stat_label (_("Length:"), table, 2, top+1);
	do_stat_label (g_strdup_printf ("%d", gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_LEN, NULL, index)),
		table, 3, top+1);
	do_stat_label (_("Expiration Date:"), table, 0, top+2);
	
	expires = gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_EXPIRE, NULL, index);
	
	if (!seahorse_context_key_has_secret (swidget->sctx, skey)) {
		if (expires)
			do_stat_label (seahorse_util_get_date_string (expires), table, 1, top+2);
		else
			do_stat_label (_("Never Expires"), table, 1, top+2);
	}
	else {
		date_edit = gnome_date_edit_new (expires, FALSE, FALSE);
		gtk_widget_show (date_edit);
		gtk_table_attach (table, date_edit, 1, 3, top+2, top+3, GTK_FILL, 0, 0, 0);
		gtk_widget_set_sensitive (date_edit, expires);
		
		if (index == 0)
			g_signal_connect_after (GNOME_DATE_EDIT (date_edit), "date_changed",
				G_CALLBACK (primary_expires_date_changed), swidget);
		else
			g_signal_connect_after (GNOME_DATE_EDIT (date_edit), "date_changed",
				G_CALLBACK (subkey_expires_date_changed), swidget);
		
		widget = gtk_check_button_new_with_mnemonic (_("Never E_xpires"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !expires);
		gtk_tooltips_set_tip (tooltips, widget, _("If key never expires"), NULL);
		gtk_widget_show (widget);
		gtk_table_attach (table, widget, 3, 4, top+2, top+3, GTK_FILL, 0, 0, 0);
		
		g_signal_connect_after (GTK_TOGGLE_BUTTON (widget), "toggled",
			G_CALLBACK (set_date_edit_sensitive), date_edit);
		
		if (index == 0)
			g_signal_connect_after (GTK_TOGGLE_BUTTON (widget), "toggled",
				G_CALLBACK (never_expires_primary_toggled), swidget);
		else
			g_signal_connect_after (GTK_TOGGLE_BUTTON (widget), "toggled",
				G_CALLBACK (never_expires_subkey_toggled), swidget);
	}
}

static void
clear_notebook (GtkNotebook *notebook)
{
	while (gtk_notebook_get_current_page (notebook) >= 0)
		gtk_notebook_remove_page (notebook, 0);
}

static void
do_uids (SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	GtkNotebook *notebook;
	GtkTable *table;
	GtkWidget *label, *widget;
	gint index = 0, max;
	GSList *group = NULL;
	GtkTooltips *tooltips;
	
	notebook = GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, "uids"));
	clear_notebook (notebook);
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	max = seahorse_key_get_num_uids (skwidget->skey);
	tooltips = gtk_tooltips_new ();
	
	while (index < max) {
		if (max > 1 && seahorse_context_key_has_secret (swidget->sctx, skwidget->skey)) {
			table = GTK_TABLE (gtk_table_new (2, 2, FALSE));
			
			widget = gtk_radio_button_new_with_mnemonic (group, _("_Primary"));
			gtk_tooltips_set_tip (tooltips, widget, _("Set as primary User ID"), NULL);
			group = g_slist_append (group, widget);
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
		
		widget = gtk_label_new (seahorse_key_get_userid (skwidget->skey, index));
		gtk_table_attach (table, widget, 0, 3, 0, 1, GTK_FILL, 0, 0, 0);
		
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		
		label = gtk_label_new (g_strdup_printf (_("User ID %d"), (index+1)));
		gtk_notebook_append_page (notebook, GTK_WIDGET (table), label);
		
		index++;
	}
	
	gtk_widget_show_all (GTK_WIDGET (notebook));
}

static void
do_subkeys (SeahorseWidget *swidget)
{
	SeahorseKeyWidget *skwidget;
	GtkNotebook *notebook;
	GtkTable *table;
	GtkWidget *label, *widget, *button;
	gint index = 1, max;
	GtkTooltips *tooltips;
	
	notebook = GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, SUBKEYS));
	clear_notebook (notebook);
	
	skwidget = SEAHORSE_KEY_WIDGET (swidget);
	max = seahorse_key_get_num_subkeys (skwidget->skey);
	tooltips = gtk_tooltips_new ();
	
	widget = glade_xml_get_widget (swidget->xml, "view_subkeys");
	if (max == 0) {
		gtk_widget_hide (widget);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), FALSE);
	}
	else
		gtk_widget_show (widget);
	
	while (index <= max) {
		if (seahorse_context_key_has_secret (swidget->sctx, skwidget->skey)) {
			table = GTK_TABLE (gtk_table_new (4, 4, FALSE));
			
			do_stat_label (_("Status:"), table, 0, 3);
			
			if (gpgme_key_get_ulong_attr (skwidget->skey->key,
			GPGME_ATTR_KEY_REVOKED, NULL, index))
				do_stat_label (_("Revoked"), table, 1, 3);
			else if (gpgme_key_get_ulong_attr (skwidget->skey->key,
			GPGME_ATTR_KEY_EXPIRED, NULL, index))
				do_stat_label (_("Expired"), table, 1, 3);
			else
				do_stat_label (_("Good"), table, 1, 3);
			
			/* Do revoke button */
			if (!gpgme_key_get_ulong_attr (skwidget->skey->key,
			GPGME_ATTR_KEY_REVOKED, NULL, index)) {
				label = gtk_hbox_new (FALSE, 0);
				/* Icon */
				widget = gtk_image_new_from_stock (GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
				gtk_container_add (GTK_CONTAINER (label), widget);
				/* Label */
				widget = gtk_label_new_with_mnemonic (_("_Revoke"));
				gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
				gtk_container_add (GTK_CONTAINER (label), widget);
				/* Alignment */
				widget = gtk_alignment_new (0.5, 0.5, 0, 0);
				gtk_container_add (GTK_CONTAINER (widget), label);
				/* Button */
				button = gtk_button_new ();
				g_signal_connect_after (GTK_BUTTON (button), "clicked",
					G_CALLBACK (revoke_subkey_clicked), swidget);
				gtk_tooltips_set_tip (tooltips, button, _("Revoke Key"), NULL);
				/* Add label & icon, put in table */
				gtk_container_add (GTK_CONTAINER (button), widget);
				gtk_widget_show_all (button);
				gtk_table_attach (table, button, 2, 3, 3, 4, GTK_FILL, 0, 0, 0);
			}
			
			/* Do delete button */
			widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
			g_signal_connect_after (GTK_BUTTON (widget), "clicked",
				G_CALLBACK (del_subkey_clicked), swidget);
			gtk_widget_show (widget);
			gtk_table_attach (table, widget, 3, 4, 3, 4, GTK_FILL, 0, 0, 0);
		}
		else {
			table = GTK_TABLE (gtk_table_new (3, 4, FALSE));
			do_stat_label (_("Status:"), table, 2, 2);
			
			if (gpgme_key_get_ulong_attr (skwidget->skey->key,
			GPGME_ATTR_KEY_REVOKED, NULL, index))
				do_stat_label (_("Revoked"), table, 3, 2);
			else if (gpgme_key_get_ulong_attr (skwidget->skey->key,
			GPGME_ATTR_KEY_EXPIRED, NULL, index))
				do_stat_label (_("Expired"), table, 3, 2);
			else
				do_stat_label (_("Good"), table, 3, 2);
		}
		
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		
		label = gtk_label_new (g_strdup_printf (_("Subkey %d"), index));
		gtk_notebook_append_page (notebook, GTK_WIDGET (table), label);
		gtk_widget_show_all (GTK_WIDGET (table));
		
		do_stats (swidget, table, 0, index);
		
		index++;
	}
}

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseWidget *swidget)
{
	switch (change) {
		case SKEY_CHANGE_UIDS:
			do_uids (swidget);
			break;
		case SKEY_CHANGE_SUBKEYS: case SKEY_CHANGE_EXPIRE:
			do_subkeys (swidget);
			break;
		default:
			break;
	}
}

/**
 * seahorse_key_properties_new:
 * @sctx: Current #SeahorseContext
 * @skey: #SeahorseKey
 *
 * Creates a new #SeahorseKeyWidget dialog for showing @skey's properties
 * and possibly editing @skey's attributes.
 **/
void
seahorse_key_properties_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseWidget *swidget;
	GtkWidget *widget;
	GtkOptionMenu *option;
	
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
	gtk_option_menu_set_history (option, seahorse_key_get_trust (skey)-1);
		
	widget = glade_xml_get_widget (swidget->xml, DISABLED);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
		gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_KEY_DISABLED, NULL, 0));
	
	do_stats (swidget, GTK_TABLE (glade_xml_get_widget (swidget->xml, "table")), 2, 0);
	do_uids (swidget);
	do_subkeys (swidget);
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_window_set_title (GTK_WINDOW (widget), seahorse_key_get_userid (skey, 0));
	
	glade_xml_signal_connect_data (swidget->xml, "export_activate",
		G_CALLBACK (export_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "delete_activate",
		G_CALLBACK (delete_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "trust_changed",
		G_CALLBACK (trust_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "disabled_toggled",
		G_CALLBACK (disabled_toggled), swidget);
	glade_xml_signal_connect_data (swidget->xml, "change_passphrase_activate",
		G_CALLBACK (change_passphrase_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_uid_activate",
		G_CALLBACK (add_uid_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_subkey_activate",
		G_CALLBACK (add_subkey_activate), swidget);
	glade_xml_signal_connect_data (swidget->xml, "subkeys_activate",
		G_CALLBACK (subkeys_activate), swidget);
	
	g_signal_connect_after (skey, "changed", G_CALLBACK (key_changed), swidget);
}
