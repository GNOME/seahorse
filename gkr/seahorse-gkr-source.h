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
 * SeahorseGkrSource: A key source for gnome-keyring credentials
 * 
 * - Derived from SeahorseSource
 * - Adds the keys it loads to the SeahorseContext.
 * 
 * Properties:
 *  ktype: (GQuark) The ktype (ie: SEAHORSE_GKR) of keys originating from this 
           key source.
 *  location: (SeahorseLocation) The location of keys that come from this 
 *         source. (ie: SEAHORSE_LOCATION_LOCAL)
 */
 
#ifndef __SEAHORSE_GKR_SOURCE_H__
#define __SEAHORSE_GKR_SOURCE_H__

#include "seahorse-source.h"

#define SEAHORSE_TYPE_GKR_SOURCE            (seahorse_gkr_source_get_type ())
#define SEAHORSE_GKR_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_SOURCE, SeahorseGkrSource))
#define SEAHORSE_GKR_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_SOURCE, SeahorseGkrSourceClass))
#define SEAHORSE_IS_GKR_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_SOURCE))
#define SEAHORSE_IS_GKR_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_SOURCE))
#define SEAHORSE_GKR_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_SOURCE, SeahorseGkrSourceClass))

typedef struct _SeahorseGkrSource SeahorseGkrSource;
typedef struct _SeahorseGkrSourceClass SeahorseGkrSourceClass;
typedef struct _SeahorseGkrSourcePrivate SeahorseGkrSourcePrivate;

struct _SeahorseGkrSource {
	GObject parent;
	SeahorseGkrSourcePrivate *pv;
};

struct _SeahorseGkrSourceClass {
	GObjectClass parent_class;
};

GType               seahorse_gkr_source_get_type          (void);

SeahorseGkrSource*  seahorse_gkr_source_new               (void);

#endif /* __SEAHORSE_GKR_SOURCE_H__ */
