/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_OBJECT_H__
#define __SEAHORSE_OBJECT_H__

/* 
 * Derived classes must set the following properties:
 * 
 * label: string: The displayable string label of this object.
 * icon: A GIcon.
 * usage: SeahorseUsage: The usage of this object.
 *  
 * The following will be calculated automatically if not set:
 * 
 * markup: string: The markup string label of this object.
 * identifier: string: The displayable key identifier for this object.
 *
 * The following can optionally be set:
 *  
 * location: SeahorseLocation: The location this object is present at. 
 * flags: guint: Flags from the SEAHORSE_FLAG_ set. 
 */

#include "seahorse-common.h"

#include <glib-object.h>

#define SEAHORSE_TYPE_OBJECT               (seahorse_object_get_type ())
#define SEAHORSE_OBJECT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_OBJECT, SeahorseObject))
#define SEAHORSE_OBJECT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_OBJECT, SeahorseObjectClass))
#define SEAHORSE_IS_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_OBJECT))
#define SEAHORSE_IS_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_OBJECT))
#define SEAHORSE_OBJECT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_OBJECT, SeahorseObjectClass))

typedef struct _SeahorseObject SeahorseObject;
typedef struct _SeahorseObjectClass SeahorseObjectClass;
typedef struct _SeahorseObjectPrivate SeahorseObjectPrivate;
    
struct _SeahorseObject {
	GObject parent;
	SeahorseObjectPrivate *pv;
};

struct _SeahorseObjectClass {
	GObjectClass parent_class;
};

GType               seahorse_object_get_type               (void);

SeahorsePlace *     seahorse_object_get_place              (SeahorseObject *self);

void                seahorse_object_set_place              (SeahorseObject *self,
                                                            SeahorsePlace *value);

const gchar*        seahorse_object_get_label              (SeahorseObject *self);

const gchar*        seahorse_object_get_markup             (SeahorseObject *self);

const gchar*        seahorse_object_get_identifier         (SeahorseObject *self);

const gchar*        seahorse_object_get_nickname           (SeahorseObject *self);

SeahorseUsage       seahorse_object_get_usage              (SeahorseObject *self);

guint               seahorse_object_get_flags              (SeahorseObject *self);

#endif /* __SEAHORSE_OBJECT_H__ */
