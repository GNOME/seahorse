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

#include "seahorse-key-dialogs.h"
#include "seahorse-key-widget.h"
#include "seahorse-util.h"
#include "seahorse-key-op.h"

#define NOTEBOOK "notebook"
#define SPACING 12

static void
trust_changed (GtkOptionMenu *optionmenu, SeahorseWidget *swidget)
{
	gint trust;
	SeahorseKey *skey;
	
	trust = gtk_option_menu_get_history (optionmenu) + 1;
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	if (trust != gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_OTRUST, NULL, 0))
		seahorse_key_op_set_trust (swidget->sctx, skey, trust);
}

static void
disabled_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	seahorse_key_op_set_disabled (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		gtk_toggle_button_get_active (togglebutton));
}

static void
do_stat_label (const gchar *label, GtkTable *table, guint left, guint top,
	       gboolean selectable, GtkTooltips *tips, const gchar *tip)
{
	GtkWidget *widget, *align;
	
	widget = gtk_label_new (label);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	gtk_label_set_selectable (GTK_LABEL (widget), selectable);
	
	align = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER (align), widget);
	
	if (tip != NULL) {
		widget = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (widget), align);
		gtk_tooltips_set_tip (tips, widget, tip, NULL);
	}
	else
		widget = align;
	
	gtk_table_attach (table, widget, left, left+1, top, top+1, GTK_FILL, 0, 0, 0);
	gtk_widget_show_all (widget);
}

static GtkWidget*
do_stat_button (gchar *label, const gchar *stock_id)
{
	GtkWidget *widget, *container;
	
	container = gtk_hbox_new (FALSE, 0);
	/* Icon */
	widget = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_BUTTON);
	gtk_container_add (GTK_CONTAINER (container), widget);
	/* Label */
	widget = gtk_label_new_with_mnemonic (label);
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	gtk_container_add (GTK_CONTAINER (container), widget);
	/* Alignment */
	widget = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (container));
	/* Button, add label & icon */
	container = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show_all (container);
	
	return container;
}

static void
do_uids (SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint uid, max;
	GtkTooltips *tips;
	GtkNotebook *nb;
	GtkTable *table;
	GtkWidget *widget, *align;
	GtkContainer *box;
	GSList *group = NULL;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	max = seahorse_key_get_num_uids (skey);
	tips = gtk_tooltips_new ();
	/* do uid page */
	widget = glade_xml_get_widget (swidget->xml, NOTEBOOK);
	gtk_notebook_remove_page (GTK_NOTEBOOK (widget), 1);
	
	if (max > 1) {
		nb = GTK_NOTEBOOK (gtk_notebook_new ());
		gtk_notebook_set_scrollable (nb, TRUE);
		gtk_notebook_popup_enable (nb);
		gtk_widget_show (GTK_WIDGET (nb));
		gtk_container_set_border_width (GTK_CONTAINER (nb), SPACING);
		gtk_notebook_insert_page (GTK_NOTEBOOK (widget), GTK_WIDGET (nb),
			gtk_label_new (_("User IDs")), 1);
	}
	
	/* foreach uid */
	for (uid = 0; uid < max; uid++) {
		/* add notebook page */
		box = GTK_CONTAINER (gtk_vbox_new (FALSE, SPACING));
		gtk_container_set_border_width (box, SPACING);
		
		if (max > 1) {
			gtk_notebook_append_page (nb, GTK_WIDGET (box),
				gtk_label_new (g_strdup_printf (_("User ID %d"), uid+1)));
		}
		else {
			gtk_notebook_insert_page (GTK_NOTEBOOK (widget), GTK_WIDGET (box),
				gtk_label_new (_("User ID")), 1);
		}
		
		/***** do uid label & options *****/
		
		table = GTK_TABLE (gtk_table_new (2, 3, FALSE));
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_add (box, GTK_WIDGET (table));
		/* do uid */
		widget = gtk_label_new (seahorse_key_get_userid (skey, uid));
		gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
		align = gtk_alignment_new (0, 0.5, 0, 0);
		gtk_container_add (GTK_CONTAINER (align), widget);
		gtk_table_attach (table, align, 0, 3, 0, 1, GTK_FILL, 0, 0, 0);
		/* do primary */
		if (SEAHORSE_IS_KEY_PAIR (skey)) {
			widget = gtk_radio_button_new_with_mnemonic (group, _("_Primary"));
			//callback
			gtk_tooltips_set_tip (tips, widget, _("Set as primary User ID"), NULL);
			group = g_slist_append (group, widget);
			gtk_widget_set_sensitive (widget, FALSE);
			gtk_table_attach (table, widget, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
		}
		/* do sign */
		widget = do_stat_button (_("_Sign"), GTK_STOCK_INDEX);
		//callback
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
		/* do delete */
		widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
		//callback
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);
		
		/***** do ciphers *****/
		
		widget = gtk_frame_new (_("Ciphers"));
		gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
		gtk_container_add (box, widget);
		table = GTK_TABLE (gtk_table_new (2, 4, FALSE));
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (table));
		
		widget = gtk_check_button_new_with_mnemonic ("_3DES");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("_CAST5");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("_BLOWFISH");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("_AES");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 3, 4, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("AES_192");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("AES_256");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("_TWOFISH");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 2, 3, 1, 2, GTK_FILL, 0, 0, 0);
		
		/***** do hashes *****/
		
		widget = gtk_frame_new (_("Hashes"));
		gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
		gtk_container_add (box, widget);
		table = GTK_TABLE (gtk_table_new (1, 3, FALSE));
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (table));
		
		widget = gtk_check_button_new_with_mnemonic ("_MD5");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("S_HA1");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("_RIPEMD160");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);
		
		/***** do compression *****/
		
		widget = gtk_frame_new (_("Compression"));
		gtk_frame_set_shadow_type (GTK_FRAME (widget), GTK_SHADOW_NONE);
		gtk_container_add (box, widget);
		table = GTK_TABLE (gtk_table_new (1, 3, FALSE));
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (table));
		
		widget = gtk_check_button_new_with_mnemonic ("_Uncompression");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("_ZIP");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);
		
		widget = gtk_check_button_new_with_mnemonic ("Z_LIB");
		gtk_widget_set_sensitive (widget, FALSE);
		gtk_table_attach (table, widget, 2, 3, 0, 1, GTK_FILL, 0, 0, 0);
		
		gtk_widget_show_all (GTK_WIDGET (box));
	}
}

static gint
get_subkey_index (SeahorseWidget *swidget)
{
	return (gtk_notebook_get_current_page (GTK_NOTEBOOK (
		glade_xml_get_widget (swidget->xml, NOTEBOOK))) - 1);
}

static void
primary_expires_date_changed (GnomeDateEdit *gde, SeahorseWidget *swidget)
{
	seahorse_key_pair_op_set_expires (swidget->sctx, SEAHORSE_KEY_PAIR (
		SEAHORSE_KEY_WIDGET (swidget)->skey), 0, gnome_date_edit_get_time (gde));
}

static void
subkey_expires_date_changed (GnomeDateEdit *gde, SeahorseWidget *swidget)
{
	seahorse_key_pair_op_set_expires (swidget->sctx, SEAHORSE_KEY_PAIR (
		SEAHORSE_KEY_WIDGET (swidget)->skey), get_subkey_index (swidget),
		gnome_date_edit_get_time (gde));
}

static void
set_date_edit_sensitive (GtkToggleButton *togglebutton, GtkWidget *widget)
{
	gtk_widget_set_sensitive (widget, !gtk_toggle_button_get_active (togglebutton));
}

static void
never_expires_primary_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	
	/* if want to never expire & expires, set to never expires */
	if (gtk_toggle_button_get_active (togglebutton) && gpgme_key_get_ulong_attr (
	skey->key, GPGME_ATTR_EXPIRE, NULL, 0)) {
		seahorse_key_pair_op_set_expires (swidget->sctx, SEAHORSE_KEY_PAIR (skey), 0, 0);
	}
}

static void
never_expires_subkey_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint index;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	index = get_subkey_index (swidget);
	
	/* if want to never expire & expires, set to never expires */
	if (gtk_toggle_button_get_active (togglebutton) && gpgme_key_get_ulong_attr (
	skey->key, GPGME_ATTR_EXPIRE, NULL, index)) {
		seahorse_key_pair_op_set_expires (swidget->sctx, SEAHORSE_KEY_PAIR (skey), index, 0);
	}
}

static void
do_stats (SeahorseWidget *swidget, GtkTable *table, guint top, guint index)
{
	SeahorseKey *skey;
	time_t expires;
	GtkWidget *widget, *date_edit;
	GtkTooltips *tips;
	const gchar *str;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	tips = gtk_tooltips_new ();
	/* key id */
	do_stat_label (_("Key ID:"), table, 0, top, FALSE, tips, _("Key identifier"));
	
	do_stat_label (seahorse_key_get_keyid (skey, index), table, 1, top, TRUE,
		tips, gpgme_key_get_string_attr (skey->key, GPGME_ATTR_KEYID, NULL, index));
	/* type */
	do_stat_label (_("Type:"), table, 2, top, FALSE, tips, _("Algorithm"));
	str = gpgme_key_get_string_attr (skey->key, GPGME_ATTR_ALGO, NULL, index);
	if (g_str_equal ("ElG", str))
		str = "ElGamal";
	do_stat_label (str, table, 3, top, FALSE, NULL, NULL);
	/* created */
	do_stat_label (_("Created:"), table, 0, top+1, FALSE, tips, _("Key creation date"));
	do_stat_label (seahorse_util_get_date_string (gpgme_key_get_ulong_attr (
		skey->key, GPGME_ATTR_CREATED, NULL, index)), table, 1, top+1, FALSE, NULL, NULL);
	/* length */
	do_stat_label (_("Length:"), table, 2, top+1, FALSE, NULL, NULL);
	do_stat_label (g_strdup_printf ("%d", gpgme_key_get_ulong_attr (skey->key,
		GPGME_ATTR_LEN, NULL, index)), table, 3, top+1, FALSE, NULL, NULL);
	/* status */
	do_stat_label (_("Status:"), table, 0, top+3, FALSE, NULL, NULL);
	str = _("Good");
	if (gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_KEY_REVOKED, NULL, index))
		str = _("Revoked");
	else if (gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_KEY_EXPIRED, NULL, index))
		str = _("Expired");
	do_stat_label (str, table, 1, top+3, FALSE, NULL, NULL);
	/* expires */
	do_stat_label (_("Expiration Date:"), table, 0, top+2, FALSE, NULL, NULL);
	
	expires = gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_EXPIRE, NULL, index);
	
	if (!SEAHORSE_IS_KEY_PAIR (skey)) {
		if (expires)
			do_stat_label (seahorse_util_get_date_string (expires),
				table, 1, top+2, FALSE, NULL, NULL);
		else
			do_stat_label (_("Never Expires"), table, 1, top+2, FALSE, NULL, NULL);
	}
	else {
		date_edit = gnome_date_edit_new (expires, FALSE, FALSE);
		gtk_widget_show (date_edit);
		gtk_table_attach (table, date_edit, 1, 2, top+2, top+3, GTK_FILL, 0, 0, 0);
		gtk_widget_set_sensitive (date_edit, expires);
		
		if (index == 0)
			g_signal_connect_after (GNOME_DATE_EDIT (date_edit), "date_changed",
				G_CALLBACK (primary_expires_date_changed), swidget);
		else
			g_signal_connect_after (GNOME_DATE_EDIT (date_edit), "date_changed",
				G_CALLBACK (subkey_expires_date_changed), swidget);
		
		widget = gtk_check_button_new_with_mnemonic (_("Never E_xpires"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), !expires);
		gtk_tooltips_set_tip (tips, widget, _("If key never expires"), NULL);
		gtk_widget_show (widget);
		gtk_table_attach (table, widget, 2, 3, top+2, top+3, GTK_FILL, 0, 0, 0);
		
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
del_subkey_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	seahorse_delete_subkey_new (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		get_subkey_index (swidget));
}

static void
revoke_subkey_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	seahorse_revoke_new (swidget->sctx, SEAHORSE_KEY_WIDGET (swidget)->skey,
		get_subkey_index (swidget));
}

static void
do_subkeys (SeahorseWidget *swidget)
{
	SeahorseKey *skey;
	gint key, max;
	GtkTooltips *tips;
	GtkNotebook *nb;
	GtkTable *table;
	GtkWidget *widget;
	gint left = 0;
	
	skey = SEAHORSE_KEY_WIDGET (swidget)->skey;
	max = seahorse_key_get_num_subkeys (skey);
	g_return_if_fail (max >= 0);
	
	tips = gtk_tooltips_new ();
	nb = GTK_NOTEBOOK (glade_xml_get_widget (swidget->xml, NOTEBOOK));
	
	/* clear subkeys */
	while (gtk_notebook_get_n_pages (nb) > 2)
		gtk_notebook_remove_page (nb, 2);
	
	/* foreach subkey */
	for (key = 1; key <= max; key++) {
		table = GTK_TABLE (gtk_table_new (5, 4, FALSE));
		/* if do revoke button */
		if (SEAHORSE_IS_KEY_PAIR (skey) && !gpgme_key_get_ulong_attr (
		skey->key, GPGME_ATTR_KEY_REVOKED, NULL, key)) {
			widget = do_stat_button (_("_Revoke"), GTK_STOCK_CANCEL);
			g_signal_connect_after (GTK_BUTTON (widget), "clicked",
				G_CALLBACK (revoke_subkey_clicked), swidget);
			gtk_tooltips_set_tip (tips, widget, g_strdup_printf (
				_("Revoke subkey %d"), key), NULL);
			gtk_table_attach (table, widget, 0, 1, 4, 5, GTK_FILL, 0, 0, 0);
			left = 1;
		}
		
		gtk_table_set_row_spacings (table, SPACING);
		gtk_table_set_col_spacings (table, SPACING);
		gtk_container_set_border_width (GTK_CONTAINER (table), SPACING);
		
		widget = gtk_label_new (g_strdup_printf (_("Subkey %d"), key));
		gtk_notebook_append_page (nb, GTK_WIDGET (table), widget);
		gtk_widget_show_all (GTK_WIDGET (table));
		
		do_stats (swidget, table, 0, key);
		
		/* Do delete button */
		widget = gtk_button_new_from_stock (GTK_STOCK_DELETE);
		g_signal_connect_after (GTK_BUTTON (widget), "clicked",
			G_CALLBACK (del_subkey_clicked), swidget);
		gtk_widget_show (widget);
		gtk_tooltips_set_tip (tips, widget, g_strdup_printf (
			_("Delete subkey %d"), key), NULL);
		gtk_table_attach (table, widget, left, left+1, 4, 5, GTK_FILL, 0, 0, 0);
	}
}

static void
key_changed (SeahorseKey *skey, SeahorseKeyChange change, SeahorseWidget *swidget)
{
	switch (change) {
		case SKEY_CHANGE_UIDS:
			do_uids (swidget);
			break;
		case SKEY_CHANGE_SUBKEYS: case SKEY_CHANGE_EXPIRES:
			do_subkeys (swidget);
			break;
		default:
			break;
	}
}

static void
properties_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
	g_signal_handlers_disconnect_by_func ( SEAHORSE_KEY_WIDGET (swidget)->skey,
		key_changed, swidget));
}

void
seahorse_key_properties_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseWidget *swidget;
	GtkWidget *widget;
	
	swidget = seahorse_key_widget_new ("key-properties", sctx, skey);
	g_return_if_fail (swidget != NULL);
	
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	g_signal_connect (GTK_OBJECT (widget), "destroy", G_CALLBACK (properties_destroyed), swidget);
	g_signal_connect_after (skey, "changed", G_CALLBACK (key_changed), swidget);
	
	do_stats (swidget, GTK_TABLE (glade_xml_get_widget (swidget->xml, "primary_table")), 2, 0);
	do_uids (swidget);
	do_subkeys (swidget);
	
	gtk_label_set_text (GTK_LABEL (glade_xml_get_widget (swidget->xml, "fingerprint")),
		seahorse_key_get_fingerprint (skey));
	
	/* disable trust options */
	if (SEAHORSE_IS_KEY_PAIR (skey))
		widget = glade_xml_get_widget (swidget->xml, "unknown");
	else
		widget = glade_xml_get_widget (swidget->xml, "ultimate");
	gtk_widget_set_sensitive (widget, FALSE);
	
	gtk_option_menu_set_history (GTK_OPTION_MENU (glade_xml_get_widget (swidget->xml, "trust")),
		seahorse_key_get_trust (skey) - 1);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, "disabled")),
		gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_KEY_DISABLED, NULL, 0));
	
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
		g_strdup_printf (_("%s Properties"), seahorse_key_get_userid (skey, 0)));
	
	glade_xml_signal_connect_data (swidget->xml, "trust_changed",
		G_CALLBACK (trust_changed), swidget);
	glade_xml_signal_connect_data (swidget->xml, "disabled_toggled",
		G_CALLBACK (disabled_toggled), swidget);
}
