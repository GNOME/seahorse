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
 
/** 
 * A collection of functions for accessing gconf. Adapted from libeel. 
 */
  
#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <gtk/gtk.h>

#define PGP_SCHEMAS "/desktop/pgp"
#define ARMOR_KEY PGP_SCHEMAS "/ascii_armor"
#define DEFAULT_KEY PGP_SCHEMAS "/default_key"
#define ENCRYPTSELF_KEY PGP_SCHEMAS "/encrypt_to_self"
#define LASTSIGNER_KEY PGP_SCHEMAS "/last_signer"
#define MULTI_EXTENSION_KEY PGP_SCHEMAS "/package_extension"
#define MULTI_SEPERATE_KEY PGP_SCHEMAS "/multi_seperate"
#define KEYSERVER_KEY PGP_SCHEMAS "/keyservers/all_keyservers"
#define LASTSEARCH_KEY PGP_SCHEMAS "/keyservers/search_text"
#define LASTSERVERS_KEY PGP_SCHEMAS "/keyservers/search_keyservers"
#define PUBLISH_TO_KEY PGP_SCHEMAS "/keyservers/publish_to"

#define SEAHORSE_SCHEMAS            "/apps/seahorse"

#define KEYSHARING_KEY              SEAHORSE_SCHEMAS "/sharing/sharing_enabled"

void            seahorse_gconf_set_boolean       (const char         *key, 
                                                  gboolean           boolean_value);

gboolean        seahorse_gconf_get_boolean       (const char         *key);

void            seahorse_gconf_set_integer       (const char         *key, 
                                                  int                int_value);

int             seahorse_gconf_get_integer       (const char         *key);

void            seahorse_gconf_set_string        (const char         *key, 
                                                  const char         *string_value);

char*           seahorse_gconf_get_string        (const char         *key);

void            seahorse_gconf_set_string_list   (const char         *key, 
                                                  const GSList       *slist);

GSList*         seahorse_gconf_get_string_list   (const char         *key);

GConfEntry*     seahorse_gconf_get_entry         (const char         *key);

guint           seahorse_gconf_notify            (const char         *key, 
                                                  GConfClientNotifyFunc notification_callback,
                                                  gpointer           callback_data);

void            seahorse_gconf_notify_lazy       (const char         *key,
                                                  GConfClientNotifyFunc notification_callback,
                                                  gpointer           callback_data,
                                                  GtkWidget          *lifetime);

void            seahorse_gconf_unnotify          (guint              notification_id);
