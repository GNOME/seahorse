/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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
 
#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
 
#include "seahorse-object-widget.h"
#include "seahorse-libdialogs.h"
#include "seahorse-util.h"

#include "seahorse-pgp-dialogs.h"
#include "seahorse-pgp-key-op.h"

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	GtkWidget *widget; 
	SeahorsePgpSubkey *subkey;
	gpgme_error_t err;
	time_t expiry = 0;
	struct tm t;
	
	subkey = SEAHORSE_PGP_SUBKEY (g_object_get_data (G_OBJECT (swidget), "subkey"));
	
	widget = glade_xml_get_widget (swidget->xml, "expire");
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
		
		memset (&t, 0, sizeof(t));            
		widget = glade_xml_get_widget (swidget->xml, "calendar");
		gtk_calendar_get_date (GTK_CALENDAR (widget), (guint*)&(t.tm_year), 
		                       (guint*)&(t.tm_mon), (guint*)&(t.tm_mday));
		t.tm_year -= 1900;
		expiry = mktime (&t);
	}
	
	widget = seahorse_widget_get_widget (swidget, "all-controls");
	gtk_widget_set_sensitive (widget, FALSE);
	g_object_ref (swidget);
	g_object_ref (subkey);
	
	if (expiry != seahorse_pgp_subkey_get_expires (subkey)) {
		err = seahorse_pgp_key_op_set_expires (subkey, expiry);
		if (!GPG_IS_OK (err))
			seahorse_pgp_handle_gpgme_error (err, _("Couldn't change expiry date"));
	}
    
	g_object_unref (subkey);
	g_object_unref (swidget);
	seahorse_widget_destroy (swidget);
}

void
expires_toggled (GtkWidget *widget, SeahorseWidget *swidget)
{
	GtkWidget *expire;
	GtkWidget *cal;
	
	expire = glade_xml_get_widget (swidget->xml, "expire");
	cal = glade_xml_get_widget (swidget->xml, "calendar");

	gtk_widget_set_sensitive (cal, !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (expire)));
}

void
seahorse_pgp_expires_new (SeahorsePgpSubkey *subkey, GtkWindow *parent)
{
	SeahorseWidget *swidget;
	GtkWidget *date, *expire;
	gulong expires;
	gchar *label, *title;
	
	g_return_if_fail (subkey != NULL && SEAHORSE_IS_PGP_SUBKEY (subkey));

	swidget = seahorse_widget_new_allow_multiple ("expires", parent);
	g_return_if_fail (swidget != NULL);
	g_object_set_data_full (G_OBJECT (swidget), "subkey", subkey, g_object_unref);
	
	glade_xml_signal_connect_data (swidget->xml, "on_calendar_change_button_clicked",
	                               G_CALLBACK (ok_clicked), swidget);
    
	date = glade_xml_get_widget (swidget->xml, "calendar");    
	g_return_if_fail (date != NULL);

	expire = glade_xml_get_widget (swidget->xml, "expire");
	glade_xml_signal_connect_data (swidget->xml, "on_expire_toggled",
	                               G_CALLBACK (expires_toggled), swidget);
	
	expires = seahorse_pgp_subkey_get_expires (subkey); 
	if (!expires) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (expire), TRUE);
		gtk_widget_set_sensitive (date, FALSE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (expire), FALSE);
		gtk_widget_set_sensitive (date, TRUE);
	}
    
	if (expires) {
		struct tm t;
		time_t time = (time_t)expires;
		if (gmtime_r (&time, &t)) {
			gtk_calendar_select_month (GTK_CALENDAR (date), t.tm_mon, t.tm_year + 1900);
			gtk_calendar_select_day (GTK_CALENDAR (date), t.tm_mday);
		}
	}
	
	label = seahorse_pgp_subkey_get_description (subkey);
	title = g_strdup_printf (_("Expiry: %s"), label);
	gtk_window_set_title (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)), title);
	g_free (title);
	g_free (label);
}
