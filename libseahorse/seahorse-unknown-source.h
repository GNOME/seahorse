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
 
#ifndef __SEAHORSE_UNKNOWN_SOURCE_H__
#define __SEAHORSE_UNKNOWN_SOURCE_H__

#include "seahorse-object.h"
#include "seahorse-source.h"

#define SEAHORSE_TYPE_UNKNOWN_SOURCE            (seahorse_unknown_source_get_type ())
#define SEAHORSE_UNKNOWN_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_UNKNOWN_SOURCE, SeahorseUnknownSource))
#define SEAHORSE_UNKNOWN_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_UNKNOWN_SOURCE, SeahorseUnknownSourceClass))
#define SEAHORSE_IS_UNKNOWN_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_UNKNOWN_SOURCE))
#define SEAHORSE_IS_UNKNOWN_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_UNKNOWN_SOURCE))
#define SEAHORSE_UNKNOWN_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_UNKNOWN_SOURCE, SeahorseUnknownSourceClass))

typedef struct _SeahorseUnknownSource SeahorseUnknownSource;
typedef struct _SeahorseUnknownSourceClass SeahorseUnknownSourceClass;
typedef struct _SeahorseUnknownSourcePrivate SeahorseUnknownSourcePrivate;

/**
 * SeahorseUnknownSource:
 * @parent: The parent #GObject
 * @ktype: The #GQuark key type for the source
 *
 * A  source for unknown objects
 *
 * - Derived from SeahorseSource
 * - Is used for objects that haven't been found on a key server.
 *
 * Properties:
 *  ktype: (GQuark) The ktype (ie: SKEY_UNKNOWN) of keys originating from this
           key source.
 *  location: (SeahorseLocation) The location of objects that come from this
 *         source. (ie: SEAHORSE_LOCATION_MISSING)
 */

struct _SeahorseUnknownSource {
	GObject parent;
	GQuark ktype;
};

struct _SeahorseUnknownSourceClass {
	GObjectClass parent_class;
};

GType                    seahorse_unknown_source_get_type      (void);

SeahorseUnknownSource*   seahorse_unknown_source_new           (GQuark ktype);

SeahorseObject*          seahorse_unknown_source_add_object    (SeahorseUnknownSource *self,
                                                                GQuark id,
                                                                GCancellable *cancellable);

#endif /* __SEAHORSE_UNKNOWN_SOURCE_H__ */
