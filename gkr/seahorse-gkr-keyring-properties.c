/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-keyring.h"

#include <glib/gi18n.h>

#include "seahorse-bind.h"
#include "seahorse-gtkstock.h"
#include "seahorse-object.h"
#include "seahorse-object-widget.h"
#include "seahorse-util.h"

/* -----------------------------------------------------------------------------
 * MAIN TAB 
 */

static gboolean
transform_keyring_created (const GValue *from, GValue *to)
{
	GnomeKeyringInfo *info;
	time_t time;
	
	info = g_value_get_boxed (from);
	if (info) {
		time = gnome_keyring_info_get_ctime (info);
		g_value_take_string (to, seahorse_util_get_display_date_string (time));
	}
	
	if (!g_value_get_string (to))
		g_value_set_string (to, "");
	
	return TRUE;
}

static void
setup_main (SeahorseWidget *swidget)
{
	SeahorseObject *object;
	
	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;

	/* Setup the image properly */
	seahorse_bind_property ("icon", object, "stock", 
	                        seahorse_widget_get_widget (swidget, "keyring-image"));

	/* The window title */
	seahorse_bind_property ("label", object, "title", 
	                        seahorse_widget_get_toplevel (swidget));

	/* Setup the label properly */
	seahorse_bind_property ("keyring-name", object, "label", 
	                        seahorse_widget_get_widget (swidget, "name-field"));

	/* The date field */
	seahorse_bind_property_full ("keyring-info", object, transform_keyring_created, "label", 
	                             seahorse_widget_get_widget (swidget, "created-field"), NULL);
}

/* -----------------------------------------------------------------------------
 * GENERAL
 */

G_MODULE_EXPORT void
on_keyring_properties_response (GtkDialog *dialog, int response, SeahorseWidget *swidget)
{
	if (response == GTK_RESPONSE_HELP) {
		seahorse_widget_show_help (swidget);
		return;
	}
   
	seahorse_widget_destroy (swidget);
}

void
seahorse_gkr_keyring_properties_show (SeahorseGkrKeyring *gkr, GtkWindow *parent)
{
	SeahorseObject *object = SEAHORSE_OBJECT (gkr);
	SeahorseWidget *swidget = NULL;
	
	swidget = seahorse_object_widget_new ("gkr-keyring", parent, object);
    
	/* This happens if the window is already open */
	if (swidget == NULL)
		return;
	setup_main (swidget);
}
