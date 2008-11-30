/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef __SEAHORSE_GKR_KEYRING_H__
#define __SEAHORSE_GKR_KEYRING_H__

#include <glib-object.h>

#include "seahorse-object.h"

#include <gnome-keyring.h>

#define SEAHORSE_TYPE_GKR_KEYRING               (seahorse_gkr_keyring_get_type ())
#define SEAHORSE_GKR_KEYRING(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_KEYRING, SeahorseGkrKeyring))
#define SEAHORSE_GKR_KEYRING_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_KEYRING, SeahorseGkrKeyringClass))
#define SEAHORSE_IS_GKR_KEYRING(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_KEYRING))
#define SEAHORSE_IS_GKR_KEYRING_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_KEYRING))
#define SEAHORSE_GKR_KEYRING_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_KEYRING, SeahorseGkrKeyringClass))

typedef struct _SeahorseGkrKeyring SeahorseGkrKeyring;
typedef struct _SeahorseGkrKeyringClass SeahorseGkrKeyringClass;
typedef struct _SeahorseGkrKeyringPrivate SeahorseGkrKeyringPrivate;
    
struct _SeahorseGkrKeyring {
	SeahorseObject parent;
	SeahorseGkrKeyringPrivate *pv;
};

struct _SeahorseGkrKeyringClass {
	SeahorseObjectClass parent_class;
};

GType                seahorse_gkr_keyring_get_type         (void);

SeahorseGkrKeyring*  seahorse_gkr_keyring_new              (const gchar *keyring_name);

const gchar*         seahorse_gkr_keyring_get_name         (SeahorseGkrKeyring *self);

GnomeKeyringInfo*    seahorse_gkr_keyring_get_info         (SeahorseGkrKeyring *self);

void                 seahorse_gkr_keyring_set_info         (SeahorseGkrKeyring *self,
                                                            GnomeKeyringInfo *info);

#endif /* __SEAHORSE_GKR_KEYRING_H__ */
