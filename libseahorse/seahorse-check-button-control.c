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
        GConfValue *value = gconf_entry_get_value (entry);
        if (value)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                          gconf_value_get_bool (value));
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
    GConfEntry *entry;
    
    g_return_if_fail (GTK_IS_CHECK_BUTTON (check));
    g_return_if_fail (gconf_key && *gconf_key);
    
    g_object_set_data_full (G_OBJECT (check), "gconf-key", g_strdup (gconf_key), g_free);
    seahorse_gconf_notify_lazy (gconf_key, (GConfClientNotifyFunc)gconf_notify, 
                                check, GTK_WIDGET (check));
    g_signal_connect (check, "toggled", G_CALLBACK (check_toggled), NULL); 
    
    /* Load initial value */
    entry = seahorse_gconf_get_entry (gconf_key);
    g_return_if_fail (entry != NULL);
    gconf_notify (NULL, 0, entry, check);
}
