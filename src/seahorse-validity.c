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

#include "seahorse-validity.h"

gchar*
seahorse_validity_get_string (GpgmeValidity validity)
{
	gchar *string;
	
	switch (validity) {
		case GPGME_VALIDITY_UNKNOWN:
			string = _("Unknown");
			break;
		case GPGME_VALIDITY_NEVER:
			string = _("Never");
			break;
		case GPGME_VALIDITY_MARGINAL:
			string = _("Marginal");
			break;
		case GPGME_VALIDITY_FULL:
			string = _("Full");
			break;
		case GPGME_VALIDITY_ULTIMATE:
			string = _("Ultimate");
			break;
		default:
			string = NULL;
			break;
	}
	
	return string;
}

gchar*
seahorse_validity_get_validity_from_key (const SeahorseKey *skey)
{
	GpgmeValidity validity;
	
	validity = gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_VALIDITY, NULL, 0);
	return seahorse_validity_get_string (validity);
}

gchar*
seahorse_validity_get_trust_from_key (const SeahorseKey *skey)
{
	GpgmeValidity trust;
	
	trust = gpgme_key_get_ulong_attr (skey->key, GPGME_ATTR_OTRUST, NULL, 0);
	return seahorse_validity_get_string (trust);
}

void
seahorse_validity_load_menu (GtkOptionMenu *option)
{
	GtkWidget *menu;
	GpgmeValidity validity;
	GtkWidget *item;
	gchar *val;
	
	menu = gtk_menu_new ();
	validity = GPGME_VALIDITY_UNKNOWN;
	
	while (validity <= GPGME_VALIDITY_ULTIMATE) {
		val = seahorse_validity_get_string (validity);
		if (val != NULL) {
			item = gtk_menu_item_new_with_label (val);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		}
		
		validity++;
	}
	
	gtk_widget_show_all (menu);
	gtk_option_menu_set_menu (option, menu);
}

guint
seahorse_validity_get_index (GpgmeValidity validity)
{
	//undefined == unknown
	if (validity <= 1)
		return 0;
	else
		return (validity-1);
}

GpgmeValidity
seahorse_validity_get_from_index (guint index)
{
	if (index == 0)
		return GPGME_VALIDITY_UNKNOWN;
	else
		return (index+1);
}
