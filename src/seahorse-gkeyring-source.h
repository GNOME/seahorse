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

/** 
 * SeahorseGKeyringSource: A key source for gnome-keyring credentials
 * 
 * - Derived from SeahorseKeySource
 * - Adds the keys it loads to the SeahorseContext.
 * 
 * Properties:
 *  ktype: (GQuark) The ktype (ie: SKEY_GKEYRING) of keys originating from this 
           key source.
 *  location: (SeahorseKeyLoc) The location of keys that come from this 
 *         source. (ie: SKEY_LOC_LOCAL)
 */
 
#ifndef __SEAHORSE_GKEYRING_SOURCE_H__
#define __SEAHORSE_GKEYRING_SOURCE_H__

#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_GKEYRING_SOURCE            (seahorse_gkeyring_source_get_type ())
#define SEAHORSE_GKEYRING_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKEYRING_SOURCE, SeahorseGKeyringSource))
#define SEAHORSE_GKEYRING_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKEYRING_SOURCE, SeahorseGKeyringSourceClass))
#define SEAHORSE_IS_GKEYRING_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKEYRING_SOURCE))
#define SEAHORSE_IS_GKEYRING_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKEYRING_SOURCE))
#define SEAHORSE_GKEYRING_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKEYRING_SOURCE, SeahorseGKeyringSourceClass))

typedef struct _SeahorseGKeyringSource SeahorseGKeyringSource;
typedef struct _SeahorseGKeyringSourceClass SeahorseGKeyringSourceClass;
typedef struct _SeahorseGKeyringSourcePrivate SeahorseGKeyringSourcePrivate;

struct _SeahorseGKeyringSource {
    SeahorseKeySource parent;
    
    /*< private >*/
    SeahorseGKeyringSourcePrivate *pv;
};

struct _SeahorseGKeyringSourceClass {
    SeahorseKeySourceClass parent_class;
};

GType                    seahorse_gkeyring_source_get_type          (void);

SeahorseGKeyringSource*  seahorse_gkeyring_source_new               (const gchar *keyring_name);

const gchar*             seahorse_gkeyring_source_get_keyring_name  (SeahorseGKeyringSource *gsrc);

#endif /* __SEAHORSE_GKEYRING_SOURCE_H__ */
