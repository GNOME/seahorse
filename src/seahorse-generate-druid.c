/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins, Adam Schreiber
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
#include "seahorse-key-source.h"
#include "seahorse-key-op.h"

#define EXPIRES "expiration_date"
#define LOW_SECURITY 768
#define MED_SECURITY 1024
#define HIGH_SECURITY 2048
#define EXTRA_HIGH_SECURITY 4096
#define RL_SECURITY "low_security"
#define RM_SECURITY "med_security"
#define RH_SECURITY "high_security"
#define REH_SECURITY "extra_high_security"
#define NAME "name"
#define PASS "passphrase"
#define CONFIRM "confirm_passphrase"
#define COMMENT "comment"
#define EMAIL "email_address"
#define DRUID "keygen_druid"
#define DRUID_PINFO "druidpage_pinfo"
#define DRUID_PASS "druidpage_pass"

#if 0
void
on_name_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	GnomeDruid *gnomedruid;
	
	gnomedruid =  GNOME_DRUID (glade_xml_get_widget (swidget->xml, DRUID));
	gnome_druid_set_buttons_sensitive(gnomedruid, TRUE, FALSE, TRUE, TRUE);
}


void
passphrase_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	GnomeDruid *gnomedruid;
	gnomedruid = GNOME_DRUID (glade_xml_get_widget (swidget->xml, DRUID));
	gnome_druid_set_buttons_sensitive(gnomedruid, TRUE, FALSE, TRUE, TRUE);
}
#endif


void
never_expires_toggled (GtkToggleButton *togglebutton, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, EXPIRES),
		!gtk_toggle_button_get_active (togglebutton));
}


gboolean
on_druidpagepersonalinfo_next (GnomeDruidPage *gnomedruidpage, GtkWidget *widget, SeahorseWidget *swidget)
{
	gchar *name;
	GtkWidget *msg_dialog;
	
	name = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (
		swidget->xml, NAME)), 0, -1);
		
	if (strlen (name) < 5) {
		msg_dialog = gtk_message_dialog_new (NULL,
                                                       GTK_DIALOG_MODAL,
                                                       GTK_MESSAGE_WARNING,
                                                       GTK_BUTTONS_CLOSE,
                                                       "The key name must be greater than 4 characters in length.");
		gtk_dialog_run (GTK_DIALOG (msg_dialog));
		gtk_widget_destroy (msg_dialog);
		return TRUE;
	} else {
		return FALSE;
	}
}


gboolean
on_druidpagepassphrase_next (GnomeDruidPage *gnomedruidpage, GtkWidget *widget, SeahorseWidget *swidget)
{
	gchar *pass, *confirm;
	
	pass = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (
		swidget->xml, PASS)), 0, -1);
	confirm = gtk_editable_get_chars (GTK_EDITABLE (glade_xml_get_widget (swidget->xml, CONFIRM)), 0, -1);
	//g_print("%s\n%s\n", pass, confirm);
	if(strlen (pass) > 0 && g_str_equal (pass, confirm))
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


void
on_druid_finish (GnomeDruidPage *gnomedruidpage, GtkWidget *widget, SeahorseWidget *swidget)
{
    SeahorseKeySource *sksrc;
	const gchar *name, *email, *comment, *pass;
	gint length;
	SeahorseKeyType type;
	time_t expires;
	GtkWidget *widget2;
	gpgme_error_t err;
	
	name = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, NAME)));
	email = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, EMAIL)));
	comment = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, COMMENT)));
	pass = gtk_entry_get_text (GTK_ENTRY (glade_xml_get_widget (swidget->xml, PASS)));

	

	if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, RM_SECURITY)))){
		length = MED_SECURITY;
	}else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, RH_SECURITY)))){
		length = HIGH_SECURITY;
	}else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (glade_xml_get_widget (swidget->xml, REH_SECURITY)))){
		length = EXTRA_HIGH_SECURITY;
	}
	
	type = DSA_ELGAMAL;
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	glade_xml_get_widget (swidget->xml, "never_expires"))))
		expires = 0;
	else
		expires = gnome_date_edit_get_time (GNOME_DATE_EDIT (
			glade_xml_get_widget (swidget->xml, EXPIRES)));
	
	widget2 = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_widget_hide (widget2);
    
    /* When we update to support S/MIME this will need to change */
    sksrc = seahorse_context_get_pri_source (swidget->sctx);
    g_return_if_fail (sksrc != NULL);
	
	err = seahorse_key_op_generate (sksrc, name, email, comment,
		pass, type, length, expires);
	if (!GPG_IS_OK (err)) {
		gtk_widget_show (widget2);
		seahorse_util_handle_error (err, _("Couldn't generate key"));
	}
	else
		gtk_widget_destroy (widget2);
	
}

gboolean
on_druidpagestandard4_next (GnomeDruidPage *gnomedruidpage, GtkWidget *widget, SeahorseWidget *swidget)
{
	g_print("go to finish\n");
	GtkWidget *druidpage = glade_xml_get_widget (swidget->xml, "druidpagefinish1");
		
	gnome_druid_page_prepare (GNOME_DRUID_PAGE(druidpage));
	gtk_widget_show (druidpage);
	return FALSE;
}

void
on_druidpagesecuritylevel_prepare(GnomeDruidPage *gnomedruidpage, GtkWidget *widget, SeahorseWidget *swidget)
{
	GtkWidget *med_security = glade_xml_get_widget (swidget->xml, "med_security");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (med_security), FALSE);
}
	


void
seahorse_generate_druid_show (SeahorseContext *sctx)
{	
	SeahorseWidget *swidget;
	GtkWidget *startpage;
	
	swidget = seahorse_widget_new ("generate-druid", sctx);
	g_return_if_fail (swidget != NULL);
	
	startpage = glade_xml_get_widget (swidget->xml, "druidpagestart1");
        gtk_widget_show(startpage);
        
	glade_xml_signal_connect_data(swidget->xml, "on_druidpagepersonalinfo_next",
		G_CALLBACK (on_druidpagepersonalinfo_next), swidget);

	glade_xml_signal_connect_data(swidget->xml, "on_druidpagepassphrase_next",
		G_CALLBACK (on_druidpagepassphrase_next), swidget);
                    
	/*glade_xml_signal_connect_data(swidget->xml, "on_name_changed",
		G_CALLBACK (on_name_changed), swidget);
		
	glade_xml_signal_connect_data(swidget->xml, "passphrase_changed",
		G_CALLBACK (passphrase_changed), swidget);
        */           
	glade_xml_signal_connect_data(swidget->xml, "never_expires_toggled",
		G_CALLBACK (never_expires_toggled), swidget);
                    
	glade_xml_signal_connect_data(swidget->xml, "on_druid_finish",
		G_CALLBACK (on_druid_finish), swidget);
		
	glade_xml_signal_connect_data(swidget->xml, "on_druidpagestandard4_next", 
		G_CALLBACK (on_druidpagestandard4_next), swidget);

	glade_xml_signal_connect_data(swidget->xml, "on_druidpagesecuritylevel_prepare", 
		G_CALLBACK (on_druidpagesecuritylevel_prepare), swidget);
}
