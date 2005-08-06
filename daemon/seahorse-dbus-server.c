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
#include "seahorse-service.h"

#include "seahorse-dbus-bindings.h"
#include <dbus/dbus-glib-bindings.h>

#define APPLICATION_SERVICE_NAME "org.gnome.seahorse.CryptoService"

/* The main service object */
SeahorseService *the_service = NULL;

void 
seahorse_dbus_server_init ()
{
    DBusGConnection *connection;
    DBusGProxy *driver_proxy;
    GError *err = NULL;
    guint request_name_result;

    connection = dbus_g_bus_get (DBUS_BUS_STARTER, &err);
    if (connection == NULL) {
        g_warning ("DBUS Service registration failed.");
        g_error_free (err);
		return;
	}

    driver_proxy = dbus_g_proxy_new_for_name (connection, DBUS_SERVICE_DBUS,
                                              DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name (driver_proxy, APPLICATION_SERVICE_NAME,
						0, &request_name_result, &err)) {
		g_warning ("DBUS Service name request failed.");
		g_clear_error (&err);
	}

    if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
        g_warning ("DBUS Service already started elsewhere");
		return;
	}

    if (!the_service)
        the_service = g_object_new (SEAHORSE_TYPE_SERVICE, NULL);
    
#if DBUS_VERSION == 33
    dbus_g_object_class_install_info (G_OBJECT_GET_CLASS (the_service),
                                     &dbus_glib_seahorse_service_object_info);
#else
    dbus_g_object_type_install_info (SEAHORSE_TYPE_SERVICE, 
                                     &dbus_glib_seahorse_service_object_info);
#endif
    
    dbus_g_connection_register_g_object (connection, "/org/gnome/seahorse/Crypto/Service",
                                         G_OBJECT (the_service));
}


void 
seahorse_dbus_server_cleanup ()
{
    /* TODO: Cleanup the_service object here. Need to check that DBUS
       glib bindings are okay with just destroying the object. */   
}
