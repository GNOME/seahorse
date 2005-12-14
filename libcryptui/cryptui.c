/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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
#include <gtk/gtk.h>
#include <string.h>
#include <dbus/dbus-glib-bindings.h>

#include "cryptui.h"

DBusGProxy *remote_service = NULL;

static gboolean
init_remote_service ()
{
    DBusGConnection *bus;
    GError *error = NULL;
    
    if (remote_service)
        return TRUE;

    bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (!bus) {
        g_critical ("couldn't get the session bus: %s", error->message);
        g_clear_error (&error);
        return FALSE;
    }
    
    remote_service = dbus_g_proxy_new_for_name (bus, "org.gnome.seahorse.KeyService", 
                              "/org/gnome/seahorse/keys", "org.gnome.seahorse.KeyService");
            
    if (!remote_service) {
        g_critical ("couldn't connect to the dbus service");
        return FALSE;
    }

    return TRUE;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

gchar*
cryptui_key_get_base (const gchar *key)
{
    gchar *ch;
    
    /* Anything past the second colon is stripped */
    ch = strchr (key, ':');
    if (ch) {
        ch = strchr (ch + 1, ':');
        if (ch)
            return g_strndup (key, ch - key);
    }
    
    /* Either already base or invalid */
    return g_strdup (key);
}

gchar*
cryptui_key_get_display_name (const gchar *key)
{
    GError *error = NULL;
    GValue value;
    gchar *name;
    
    if (!init_remote_service ())
        return NULL;
    
    memset (&value, 0, sizeof (value));
    
    if (!dbus_g_proxy_call (remote_service, "GetKeyField", &error,
                            G_TYPE_STRING, key, G_TYPE_STRING, "display-name", G_TYPE_INVALID, 
                            G_TYPE_VALUE, &value, G_TYPE_INVALID)) {
        g_warning ("dbus call to get display name failed: %s", error ? error->message : "");
        g_clear_error (&error);
        return NULL;
    }

    g_return_val_if_fail (G_VALUE_TYPE (&value) == G_TYPE_STRING, NULL);
    name = g_value_dup_string (&value);
    g_value_unset (&value);
    return name;
}

gchar*
cryptui_key_get_display_id (const gchar *key)
{
    GError *error = NULL;
    GValue value;
    gchar *name;
    
    if (!init_remote_service ())
        return NULL;
    
    memset (&value, 0, sizeof (value));
    
    if (!dbus_g_proxy_call (remote_service, "GetKeyField", &error,
                            G_TYPE_STRING, key, G_TYPE_STRING, "key-id", G_TYPE_INVALID, 
                            G_TYPE_VALUE, &value, G_TYPE_INVALID)) {
        g_warning ("dbus call to get display name failed: %s", error ? error->message : "");
        g_clear_error (&error);
        return NULL;
    }

    g_return_val_if_fail (G_VALUE_TYPE (&value) == G_TYPE_STRING, NULL);
    name = g_value_dup_string (&value);
    g_value_unset (&value);
    return name;
}

CryptUIEncType
cryptui_key_get_enctype (const gchar *key)
{
    /* TODO: Implement this properly */
    return CRYPTUI_ENCTYPE_PUBLIC;
}
