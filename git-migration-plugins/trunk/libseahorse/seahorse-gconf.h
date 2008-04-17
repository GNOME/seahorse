/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#define SEAHORSE_DESKTOP_KEYS           "/desktop/pgp"
#define SEAHORSE_DEFAULT_KEY            SEAHORSE_DESKTOP_KEYS "/default_key"
#define SEAHORSE_LASTSIGNER_KEY         SEAHORSE_DESKTOP_KEYS "/last_signer"
#define SEAHORSE_ENCRYPTSELF_KEY        SEAHORSE_DESKTOP_KEYS "/encrypt_to_self"
#define SEAHORSE_RECIPIENTS_SORT_KEY    SEAHORSE_DESKTOP_KEYS "/recipients/sort_by"

#define ARMOR_KEY SEAHORSE_DESKTOP_KEYS "/ascii_armor"
#define ENCRYPTSELF_KEY SEAHORSE_DESKTOP_KEYS "/encrypt_to_self"
#define MULTI_EXTENSION_KEY SEAHORSE_DESKTOP_KEYS "/package_extension"
#define MULTI_SEPERATE_KEY SEAHORSE_DESKTOP_KEYS "/multi_seperate"
#define KEYSERVER_KEY SEAHORSE_DESKTOP_KEYS "/keyservers/all_keyservers"
#define AUTORETRIEVE_KEY SEAHORSE_DESKTOP_KEYS "/keyservers/auto_retrieve"
#define AUTOSYNC_KEY SEAHORSE_DESKTOP_KEYS "/keyservers/auto_sync"
#define LASTSEARCH_KEY SEAHORSE_DESKTOP_KEYS "/keyservers/search_text"
#define LASTSERVERS_KEY SEAHORSE_DESKTOP_KEYS "/keyservers/search_keyservers"
#define PUBLISH_TO_KEY SEAHORSE_DESKTOP_KEYS "/keyservers/publish_to"

#define SEAHORSE_SCHEMAS            "/apps/seahorse"

#define KEYSHARING_KEY              SEAHORSE_SCHEMAS "/sharing/sharing_enabled"

void            seahorse_gconf_disconnect        ();

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
                                                  gpointer           lifetime);

void            seahorse_gconf_unnotify          (guint              notification_id);
