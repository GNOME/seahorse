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
 * SeahorseUnknownKey: Represents an unknown key 
 * 
 * - Derived from SeahorseKey
 * 
 * Properties:
 *   display-name: (gchar*) The display name for the key.
 *   display-id: (gchar*) The display id for the key.
 *   simple-name: (gchar*) Shortened display name for the key (for use in files etc...).
 *   fingerprint: (gchar*) Displayable fingerprint for the key.
 *   validity: (SeahorseValidity) The key validity.
 *   trust: (SeahorseValidity) Trust for the key.
 *   expires: (gulong) Date this key expires or 0.
 *   stock-id: (string) The stock icon id.
 */
 
#ifndef __SEAHORSE_UNKNOWN_KEY_H__
#define __SEAHORSE_UNKNOWN_KEY_H__

#include <gtk/gtk.h>

#include "seahorse-key.h"
#include "seahorse-unknown-source.h"

#define SEAHORSE_TYPE_UNKNOWN_KEY            (seahorse_unknown_key_get_type ())
#define SEAHORSE_UNKNOWN_KEY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_UNKNOWN_KEY, SeahorseUnknownKey))
#define SEAHORSE_UNKNOWN_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_UNKNOWN_KEY, SeahorseUnknownKeyClass))
#define SEAHORSE_IS_UNKNOWN_KEY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_UNKNOWN_KEY))
#define SEAHORSE_IS_UNKNOWN_KEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_UNKNOWN_KEY))
#define SEAHORSE_UNKNOWN_KEY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_UNKNOWN_KEY, SeahorseUnknownKeyClass))

typedef struct _SeahorseUnknownKey SeahorseUnknownKey;
typedef struct _SeahorseUnknownKeyClass SeahorseUnknownKeyClass;

struct _SeahorseUnknownKey {
    SeahorseKey parent;

    /*< public >*/
    GQuark keyid;
};

struct _SeahorseUnknownKeyClass {
    SeahorseKeyClass            parent_class;
};

GType                   seahorse_unknown_key_get_type         (void);

SeahorseUnknownKey*     seahorse_unknown_key_new              (SeahorseUnknownSource *usrc, 
                                                               GQuark keyid);

#endif /* __SEAHORSE_UNKNOWN_KEY_H__ */
