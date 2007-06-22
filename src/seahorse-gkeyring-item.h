/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 * SeahorseCredential: Represents a gnome-keyring item 
 * 
 * - Derived from SeahorseKey
 * 
 * Properties:
 *   display-name: (gchar*) The display name for the key.
 *   display-id: (gchar*) The keyid to display.
 *   simple-name: (gchar*) Shortened display name for the key (for use in files etc...).
 *   fingerprint: (gchar*) Displayable fingerprint for the key.
 *   validity: (SeahorseValidity) The key validity.
 *   trust: (SeahorseValidity) Trust for the key.
 *   expires: (gulong) Date this key expires or 0.
 *   length: (gint) The length of the key in bits.
 *   stock-id: (gpointer/string) The stock icon id.
 */
 
#ifndef __SEAHORSE_GKEYRING_ITEM_H__
#define __SEAHORSE_GKEYRING_ITEM_H__

#include <gtk/gtk.h>
#include <gnome-keyring.h>

#include "seahorse-key.h"

typedef enum {
    SEAHORSE_GKEYRING_USE_OTHER,
    SEAHORSE_GKEYRING_USE_NETWORK,
    SEAHORSE_GKEYRING_USE_WEB,
    SEAHORSE_GKEYRING_USE_PGP,
    SEAHORSE_GKEYRING_USE_SSH,
} SeahorseGKeyringUse;

#define SKEY_GKEYRING_STR                       "gnome-keyring"
#define SKEY_GKEYRING                           (g_quark_from_static_string (SKEY_GKEYRING_STR))

#define SEAHORSE_TYPE_GKEYRING_ITEM             (seahorse_gkeyring_item_get_type ())
#define SEAHORSE_GKEYRING_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKEYRING_ITEM, SeahorseGKeyringItem))
#define SEAHORSE_GKEYRING_ITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKEYRING_ITEM, SeahorseGKeyringItemClass))
#define SEAHORSE_IS_GKEYRING_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKEYRING_ITEM))
#define SEAHORSE_IS_GKEYRING_ITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKEYRING_ITEM))
#define SEAHORSE_GKEYRING_ITEM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKEYRING_ITEM_KEY, SeahorseGKeyringItemClass))


typedef struct _SeahorseGKeyringItem SeahorseGKeyringItem;
typedef struct _SeahorseGKeyringItemClass SeahorseGKeyringItemClass;

struct _SeahorseGKeyringItem {
    SeahorseKey                 parent;

    /*< public >*/
    guint32                     item_id;
    GnomeKeyringItemInfo        *info;
    GnomeKeyringAttributeList   *attributes;
};

struct _SeahorseGKeyringItemClass {
    SeahorseKeyClass            parent_class;
};

// TODO: arguments specific to GKeyring Item 
SeahorseGKeyringItem*   seahorse_gkeyring_item_new              (SeahorseKeySource *sksrc,
                                                                 guint32 item_id, 
                                                                 GnomeKeyringItemInfo *info,
                                                                 GnomeKeyringAttributeList *attributes);

GType                   seahorse_gkeyring_item_get_type         (void);

GQuark                  seahorse_gkeyring_item_get_cannonical   (guint32 item_id);

gchar*                  seahorse_gkeyring_item_get_description  (SeahorseGKeyringItem *git);

const gchar*            seahorse_gkeyring_item_get_attribute    (SeahorseGKeyringItem *git, 
                                                                 const gchar *name);

SeahorseGKeyringUse     seahorse_gkeyring_item_get_use          (SeahorseGKeyringItem *git);

#endif /* __SEAHORSE_GKEYRING_ITEM_H__ */
