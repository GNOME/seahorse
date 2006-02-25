/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, GtkCheckButton *check)
{
    const char *gconf_key = g_object_get_data (G_OBJECT (check), "gconf-key");
    if (g_str_equal (gconf_key, gconf_entry_get_key (entry))) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                        gconf_value_get_bool (gconf_entry_get_value (entry)));
    }
}

static void
check_toggled (GtkCheckButton *check, gpointer data)
{
    const char *gconf_key = g_object_get_data (G_OBJECT (check), "gconf-key");
    seahorse_gconf_set_boolean (gconf_key, 
                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));
}

void
seahorse_check_button_gconf_attach  (GtkCheckButton *check, const char *gconf_key)
{
    g_return_if_fail (GTK_IS_CHECK_BUTTON (check));
    g_return_if_fail (gconf_key && *gconf_key);
    
    g_object_set_data_full (G_OBJECT (check), "gconf-key", g_strdup (gconf_key), g_free);
    seahorse_gconf_notify_lazy (gconf_key, (GConfClientNotifyFunc)gconf_notify, 
                                check, GTK_WIDGET (check));
    g_signal_connect (check, "toggled", G_CALLBACK (check_toggled), NULL); 
}

/* -------------------------------------------------------------------------- */

enum {
    PROP_0,
    PROP_GCONF_KEY
};

G_DEFINE_TYPE (SeahorseCheckButtonControl, seahorse_check_button_control, 
               GTK_TYPE_CHECK_BUTTON);

static void
seahorse_check_button_control_init (SeahorseCheckButtonControl *ctl)
{
    
}

static void
seahorse_check_button_control_set_property (GObject *object, guint prop_id,
                                            const GValue *value, GParamSpec *pspec)
{
    SeahorseCheckButtonControl *control = SEAHORSE_CHECK_BUTTON_CONTROL (object);

    switch (prop_id) {
    case PROP_GCONF_KEY:
        seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (control), g_value_get_string (value));
        break;
    }
}

static void
seahorse_check_button_control_get_property (GObject *object, guint prop_id,
                                            GValue *value, GParamSpec *pspec)
{
    SeahorseCheckButtonControl *control = SEAHORSE_CHECK_BUTTON_CONTROL (object);
    
    switch (prop_id) {
    case PROP_GCONF_KEY:
        g_value_set_string (value, g_object_get_data (G_OBJECT (control), "gconf-key"));
        break;
    }
}

static void
seahorse_check_button_control_class_init (SeahorseCheckButtonControlClass *klass)
{
    GObjectClass *gclass;
    
    seahorse_check_button_control_parent_class = g_type_class_peek_parent (klass);
    
    gclass = G_OBJECT_CLASS (klass);
    gclass->set_property = seahorse_check_button_control_set_property;
    gclass->get_property = seahorse_check_button_control_get_property;
    
    g_object_class_install_property (gclass, PROP_GCONF_KEY,
                g_param_spec_string ("gconf_key", "GConf Key", "GConf Key to listen to",
                                     "", G_PARAM_READWRITE));
}

GtkWidget*
seahorse_check_button_control_new (const gchar *label, const gchar *gconf_key)
{
    return g_object_new (SEAHORSE_TYPE_CHECK_BUTTON_CONTROL, "label", label,
                         "use_underline", TRUE, "gconf_key", gconf_key, NULL);
}
