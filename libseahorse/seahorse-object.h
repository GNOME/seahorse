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

#ifndef __SEAHORSE_OBJECT_H__
#define __SEAHORSE_OBJECT_H__

/* 
 * Derived classes must set the following properties:
 * 
 * id: GQuark: Unique identifier for this object.
 * tag: GQuark: The module (openpgp, pkcs11, etc...) that handles this object. 
 * label: string: The displayable string label of this object.
 * icon: string: The stock ID of an icon to show for this object.
 * usage: SeahorseUsage: The usage of this object.
 *  
 * The following will be calculated automatically if not set:
 * 
 * markup: string: The markup string label of this object.
 * identifier: string: The displayable key identifier for this object.
 * description: string: The description of the type of object.
 *
 * The following can optionally be set:
 *  
 * location: SeahorseLocation: The location this object is present at. 
 * flags: guint: Flags from the SEAHORSE_FLAG_ set. 
 */

#include "seahorse-context.h"
#include "seahorse-source.h"
#include "seahorse-types.h"

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
	
	/* virtual methods ------------------------------------------------- */

	void (*realize) (SeahorseObject *self);
	
	void (*refresh) (SeahorseObject *self);
};

GType               seahorse_object_get_type               (void);

SeahorseObject*     seahorse_object_new                    (void);

void                seahorse_object_realize                (SeahorseObject *self);

void                seahorse_object_refresh                (SeahorseObject *self);

GQuark              seahorse_object_get_id                 (SeahorseObject *self);

GQuark              seahorse_object_get_tag                (SeahorseObject *self);

SeahorseSource*     seahorse_object_get_source             (SeahorseObject *self);

void                seahorse_object_set_source             (SeahorseObject *self, 
                                                            SeahorseSource *value);

SeahorseContext*    seahorse_object_get_context            (SeahorseObject *self);

SeahorseObject*     seahorse_object_get_preferred          (SeahorseObject *self);

void                seahorse_object_set_preferred          (SeahorseObject *self, 
                                                            SeahorseObject *value);

SeahorseObject*     seahorse_object_get_parent             (SeahorseObject *self);

void                seahorse_object_set_parent             (SeahorseObject *self, 
                                                            SeahorseObject *value);
 
GList*              seahorse_object_get_children           (SeahorseObject *self);

SeahorseObject*     seahorse_object_get_nth_child          (SeahorseObject *self,
                                                            guint index);

const gchar*        seahorse_object_get_label              (SeahorseObject *self);

const gchar*        seahorse_object_get_markup             (SeahorseObject *self);

const gchar*        seahorse_object_get_identifier         (SeahorseObject *self);

const gchar*        seahorse_object_get_description        (SeahorseObject *self);

const gchar*        seahorse_object_get_nickname           (SeahorseObject *self);

const gchar*        seahorse_object_get_icon               (SeahorseObject *self);


SeahorseLocation    seahorse_object_get_location           (SeahorseObject *self);

SeahorseUsage       seahorse_object_get_usage              (SeahorseObject *self);

guint               seahorse_object_get_flags              (SeahorseObject *self);

gboolean            seahorse_object_lookup_property        (SeahorseObject *self, 
                                                            const gchar *field, 
                                                            GValue *value);

typedef gboolean (*SeahorseObjectPredicateFunc) (SeahorseObject *obj, void *user_data);

typedef struct _SeahorseObjectPredicate SeahorseObjectPredicate;

struct _SeahorseObjectPredicate {
	GQuark tag;
	GQuark id;
	GType type;
	SeahorseLocation location;
	SeahorseUsage usage;
	guint flags;
	guint nflags;
	SeahorseSource *source;
	SeahorseObjectPredicateFunc custom;
	gpointer custom_target;
};

void               seahorse_object_predicate_clear         (SeahorseObjectPredicate *self);

gboolean           seahorse_object_predicate_match         (SeahorseObjectPredicate *self, 
                                                            SeahorseObject *obj);

#endif /* __SEAHORSE_OBJECT_H__ */
