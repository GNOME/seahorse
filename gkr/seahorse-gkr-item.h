/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#ifndef __SEAHORSE_GKR_ITEM_H__
#define __SEAHORSE_GKR_ITEM_H__

#include <gtk/gtk.h>
#include <gnome-keyring.h>

#include "seahorse-gkr.h"
#include "seahorse-gkr-keyring.h"

#include "seahorse-object.h"
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
typedef struct _SeahorseGkrItemPrivate SeahorseGkrItemPrivate;

struct _SeahorseGkrItem {
	SeahorseObject parent;
	SeahorseGkrItemPrivate *pv;
};

struct _SeahorseGkrItemClass {
    SeahorseObjectClass         parent_class;
};

GType                        seahorse_gkr_item_get_type           (void);

SeahorseGkrItem *            seahorse_gkr_item_new                (SeahorseGkrKeyring *keyring,
                                                                   const gchar *keyring_name,
                                                                   guint32 item_id);

void                         seahorse_gkr_item_refresh            (SeahorseGkrItem *self);

void                         seahorse_gkr_item_realize            (SeahorseGkrItem *self);

guint32                      seahorse_gkr_item_get_item_id        (SeahorseGkrItem *self);

const gchar*                 seahorse_gkr_item_get_keyring_name   (SeahorseGkrItem *self);

GnomeKeyringItemInfo*        seahorse_gkr_item_get_info           (SeahorseGkrItem *self);

void                         seahorse_gkr_item_set_info           (SeahorseGkrItem *self,
                                                                   GnomeKeyringItemInfo* info);

gboolean                     seahorse_gkr_item_has_secret         (SeahorseGkrItem *self);

const gchar*                 seahorse_gkr_item_get_secret         (SeahorseGkrItem *self);

GnomeKeyringAttributeList*   seahorse_gkr_item_get_attributes     (SeahorseGkrItem *self);

void                         seahorse_gkr_item_set_attributes     (SeahorseGkrItem *self,
                                                                   GnomeKeyringAttributeList* attrs);

const gchar*                 seahorse_gkr_item_get_attribute      (SeahorseGkrItem *self, 
                                                                   const gchar *name);

const gchar* 		     seahorse_gkr_find_string_attribute   (GnomeKeyringAttributeList *attrs, 
             		                                           const gchar *name);

SeahorseGkrUse               seahorse_gkr_item_get_use            (SeahorseGkrItem *self);

#endif /* __SEAHORSE_GKR_ITEM_H__ */
