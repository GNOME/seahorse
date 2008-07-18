
#ifndef __SEAHORSE_OBJECT_H__
#define __SEAHORSE_OBJECT_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-context.h>
#include <seahorse-source.h>
#include <stdlib.h>
#include <string.h>
#include <seahorse-types.h>

G_BEGIN_DECLS


#define SEAHORSE_TYPE_OBJECT (seahorse_object_get_type ())
#define SEAHORSE_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_OBJECT, SeahorseObject))
#define SEAHORSE_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_OBJECT, SeahorseObjectClass))
#define SEAHORSE_IS_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_OBJECT))
#define SEAHORSE_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_OBJECT))
#define SEAHORSE_OBJECT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_OBJECT, SeahorseObjectClass))

typedef struct _SeahorseObject SeahorseObject;
typedef struct _SeahorseObjectClass SeahorseObjectClass;
typedef struct _SeahorseObjectPrivate SeahorseObjectPrivate;

#define SEAHORSE_OBJECT_TYPE_CHANGE (seahorse_object_change_get_type ())
typedef struct _SeahorseObjectPredicate SeahorseObjectPredicate;
typedef gboolean (*SeahorseObjectPredicateFunc) (SeahorseObject* obj, void* user_data);

struct _SeahorseObject {
	GObject parent_instance;
	SeahorseObjectPrivate * priv;
	GQuark _tag;
	GQuark _id;
	SeahorseLocation _location;
	SeahorseUsage _usage;
	guint _flags;
	SeahorseContext* attached_to;
};

struct _SeahorseObjectClass {
	GObjectClass parent_class;
};

/* INTERFACE -------------------------------------------------- 
 TODO: Are we sure we need this? */
typedef enum  {
	SEAHORSE_OBJECT_CHANGE_ALL = 1,
	SEAHORSE_OBJECT_CHANGE_LOCATION,
	SEAHORSE_OBJECT_CHANGE_PREFERRED,
	SEAHORSE_OBJECT_CHANGE_MAX
} SeahorseObjectChange;

struct _SeahorseObjectPredicate {
	GQuark tag;
	GQuark id;
	SeahorseLocation location;
	SeahorseUsage usage;
	guint flags;
	guint nflags;
	SeahorseSource* source;
	SeahorseObjectPredicateFunc custom;
	gpointer custom_target;
};


GType seahorse_object_change_get_type (void);
GList* seahorse_object_get_children (SeahorseObject* self);
void seahorse_object_fire_changed (SeahorseObject* self, SeahorseObjectChange what);
GQuark seahorse_object_get_tag (SeahorseObject* self);
GQuark seahorse_object_get_id (SeahorseObject* self);
SeahorseLocation seahorse_object_get_location (SeahorseObject* self);
void seahorse_object_set_location (SeahorseObject* self, SeahorseLocation value);
SeahorseUsage seahorse_object_get_usage (SeahorseObject* self);
guint seahorse_object_get_flags (SeahorseObject* self);
SeahorseSource* seahorse_object_get_source (SeahorseObject* self);
void seahorse_object_set_source (SeahorseObject* self, SeahorseSource* value);
SeahorseObject* seahorse_object_get_preferred (SeahorseObject* self);
void seahorse_object_set_preferred (SeahorseObject* self, SeahorseObject* value);
char* seahorse_object_get_display_name (SeahorseObject* self);
char* seahorse_object_get_markup (SeahorseObject* self);
char* seahorse_object_get_stock_id (SeahorseObject* self);
SeahorseObject* seahorse_object_get_parent (SeahorseObject* self);
void seahorse_object_set_parent (SeahorseObject* self, SeahorseObject* value);
gboolean seahorse_object_predicate_match (SeahorseObjectPredicate *self, SeahorseObject* obj);
GType seahorse_object_get_type (void);


G_END_DECLS

#endif
