/*
 * Seahorse
 *
 * Copyright (C) 2005-2006 Stefan Walter
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

#ifndef __SEAHORSE_COLLECTION_H__
#define __SEAHORSE_COLLECTION_H__

#include <gcr/gcr.h>

#include "seahorse-predicate.h"

#define SEAHORSE_TYPE_COLLECTION               (seahorse_collection_get_type ())
#define SEAHORSE_COLLECTION(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_COLLECTION, SeahorseCollection))
#define SEAHORSE_COLLECTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_COLLECTION, SeahorseCollectionClass))
#define SEAHORSE_IS_COLLECTION(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_COLLECTION))
#define SEAHORSE_IS_COLLECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_COLLECTION))
#define SEAHORSE_COLLECTION_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_COLLECTION, SeahorseCollectionClass))

typedef struct _SeahorseCollection SeahorseCollection;
typedef struct _SeahorseCollectionClass SeahorseCollectionClass;
typedef struct _SeahorseCollectionPrivate SeahorseCollectionPrivate;

struct _SeahorseCollection {
	GObject parent;

	/* <private> */
	SeahorseCollectionPrivate *pv;
};

struct _SeahorseCollectionClass {
	GObjectClass parent_class;
};

GType                seahorse_collection_get_type             (void);

SeahorseCollection * seahorse_collection_new_for_predicate    (GcrCollection *base,
                                                               SeahorsePredicate *predicate,
                                                               GDestroyNotify destroy_func);

SeahorsePredicate *  seahorse_collection_get_predicate        (SeahorseCollection *self);

void                 seahorse_collection_refresh              (SeahorseCollection *self);

#endif /* __SEAHORSE_COLLECTION_H__ */
