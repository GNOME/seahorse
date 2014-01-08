/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_SERVERS_H__
#define __SEAHORSE_SERVERS_H__

#include <glib.h>
#include <gio/gio.h>

gchar **        seahorse_servers_get_types              (void);

gchar **        seahorse_servers_get_names              (void);

char*           seahorse_servers_get_description        (const char* type);

gchar **        seahorse_servers_get_uris               (void);

gboolean        seahorse_servers_is_valid_uri           (const char* uri);

typedef gboolean (*SeahorseValidUriFunc) (const gchar *uri);

void            seahorse_servers_register_type          (const char* type, 
                                                         const char* description, 
                                                         SeahorseValidUriFunc validate);

void            seahorse_servers_cleanup                (void);

#endif
