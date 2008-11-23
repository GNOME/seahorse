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
 *   stock-id: (string) The stock icon id.
 */
 
#ifndef __SEAHORSE_GKR_ITEM_H__
#define __SEAHORSE_GKR_ITEM_H__

#include <gtk/gtk.h>
#include <gnome-keyring.h>

#include "seahorse-gkr-module.h"

#include "seahorse-key.h"
#include "seahorse-source.h"

typedef enum {
    SEAHORSE_GKR_USE_OTHER,
    SEAHORSE_GKR_USE_NETWORK,
    SEAHORSE_GKR_USE_WEB,
    SEAHORSE_GKR_USE_PGP,
    SEAHORSE_GKR_USE_SSH,
} SeahorseGkrUse;

#define SEAHORSE_TYPE_GKR_ITEM             (seahorse_gkr_item_get_type ())
#define SEAHORSE_GKR_TYPE_ITEM SEAHORSE_TYPE_GKR_ITEM
#define SEAHORSE_GKR_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_ITEM, SeahorseGkrItem))
#define SEAHORSE_GKR_ITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_ITEM, SeahorseGkrItemClass))
#define SEAHORSE_IS_GKR_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_ITEM))
#define SEAHORSE_IS_GKR_ITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_ITEM))
#define SEAHORSE_GKR_ITEM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_ITEM_KEY, SeahorseGkrItemClass))


typedef struct _SeahorseGkrItem SeahorseGkrItem;
typedef struct _SeahorseGkrItemClass SeahorseGkrItemClass;

struct _SeahorseGkrItem {
    SeahorseKey                 parent;

    /*< public >*/
    guint32                     item_id;
    GnomeKeyringItemInfo        *info;
    GnomeKeyringAttributeList   *attributes;
    GList                       *acl;
};

struct _SeahorseGkrItemClass {
    SeahorseKeyClass            parent_class;
};

SeahorseGkrItem*        seahorse_gkr_item_new              (SeahorseSource *sksrc,
                                                            guint32 item_id, 
                                                            GnomeKeyringItemInfo *info,
                                                            GnomeKeyringAttributeList *attributes,
                                                            GList *acl);

GType                   seahorse_gkr_item_get_type         (void);

GQuark                  seahorse_gkr_item_get_cannonical   (guint32 item_id);

gchar*                  seahorse_gkr_item_get_description  (SeahorseGkrItem *git);

const gchar*            seahorse_gkr_item_get_attribute    (SeahorseGkrItem *git, 
                                                            const gchar *name);

SeahorseGkrUse          seahorse_gkr_item_get_use          (SeahorseGkrItem *git);

#endif /* __SEAHORSE_GKR_ITEM_H__ */
