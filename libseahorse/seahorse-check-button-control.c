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

#include "seahorse-check-button-control.h"
#include "seahorse-gconf.h"

enum {
	PROP_0,
	PROP_GCONF_KEY
};

static void	seahorse_check_button_control_class_init	(SeahorseCheckButtonControlClass	*klass);
static void	seahorse_check_button_control_finalize		(GObject				*gobject);
static void	seahorse_check_button_control_set_property	(GObject				*object,
								 guint					prop_id,
								 const GValue				*value,
								 GParamSpec				*pspec);
static void	seahorse_check_button_control_get_property	(GObject				*object,
								 guint					prop_id,
								 GValue					*value,
								 GParamSpec				*pspec);

static void	seahorse_check_button_control_toggled		(GtkToggleButton			*toggle);

static void	seahorse_check_button_control_gconf_notify	(GConfClient				*client,
								 guint					id,
								 GConfEntry				*entry,
								 gpointer				data);

static GtkCheckButtonClass	*parent_class	= NULL;

GType
seahorse_check_button_control_get_type (void)
{
	static GType control_type = 0;
	
	if (!control_type) {
		static const GTypeInfo control_info =
		{
			sizeof (SeahorseCheckButtonControlClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_check_button_control_class_init,
			NULL, NULL,
			sizeof (SeahorseCheckButtonControl),
			0, NULL
		};
		
		control_type = g_type_register_static (GTK_TYPE_CHECK_BUTTON, "SeahorseCheckButtonControl", &control_info, 0);
	}
	
	return control_type;
}

static void
seahorse_check_button_control_class_init (SeahorseCheckButtonControlClass *klass)
{
	GObjectClass *gobject_class;
	GtkToggleButtonClass *toggle_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	toggle_class = GTK_TOGGLE_BUTTON_CLASS (klass);
	
	gobject_class->finalize = seahorse_check_button_control_finalize;
	gobject_class->set_property = seahorse_check_button_control_set_property;
	gobject_class->get_property = seahorse_check_button_control_get_property;
	
	toggle_class->toggled = seahorse_check_button_control_toggled;
	
	g_object_class_install_property (gobject_class, PROP_GCONF_KEY,
		g_param_spec_string ("gconf_key", "GConf Key", "GConf Key to listen to",
				     "", G_PARAM_READWRITE));
}

static void
seahorse_check_button_control_finalize (GObject *gobject)
{
	SeahorseCheckButtonControl *control;
	
	control = SEAHORSE_CHECK_BUTTON_CONTROL (gobject);
	
	seahorse_gconf_unnotify (control->notify_id);
	
	g_free (control->gconf_key);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_check_button_control_set_property (GObject *object, guint prop_id,
					    const GValue *value, GParamSpec *pspec)
{
	SeahorseCheckButtonControl *control;
	
	control = SEAHORSE_CHECK_BUTTON_CONTROL (object);
	
	switch (prop_id) {
		case PROP_GCONF_KEY:
			control->gconf_key = g_strdup (g_value_get_string (value));
			control->notify_id = seahorse_gconf_notify (control->gconf_key,
				seahorse_check_button_control_gconf_notify, control);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control),
				seahorse_gconf_get_boolean (control->gconf_key));
			break;
		default:
			break;
	}
}

static void
seahorse_check_button_control_get_property (GObject *object, guint prop_id,
					    GValue *value, GParamSpec *pspec)
{
	SeahorseCheckButtonControl *control;
	
	control = SEAHORSE_CHECK_BUTTON_CONTROL (object);
	
	switch (prop_id) {
		case PROP_GCONF_KEY:
			g_value_set_string (value, control->gconf_key);
			break;
		default:
			break;
	}
}

static void
seahorse_check_button_control_toggled (GtkToggleButton *toggle)
{
	SeahorseCheckButtonControl *control;
	
	control = SEAHORSE_CHECK_BUTTON_CONTROL (toggle);
	
	seahorse_gconf_set_boolean (control->gconf_key, gtk_toggle_button_get_active (toggle));
}

static void
seahorse_check_button_control_gconf_notify (GConfClient *client, guint id,
					    GConfEntry *entry, gpointer data)
{
	SeahorseCheckButtonControl *control;
	
	control = SEAHORSE_CHECK_BUTTON_CONTROL (data);
	
	if (g_str_equal (control->gconf_key, gconf_entry_get_key (entry))) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (control),
			gconf_value_get_bool (gconf_entry_get_value (entry)));
	}
}

GtkWidget*
seahorse_check_button_control_new (const gchar *label, const gchar *gconf_key)
{
	GtkWidget *widget;
	
	widget = g_object_new (SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, "label", label,
		"use_underline", TRUE, "gconf_key", gconf_key, NULL);
	
	return widget;
}
