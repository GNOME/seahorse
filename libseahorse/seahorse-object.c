
#include <seahorse-object.h>




struct _SeahorseObjectPrivate {
	SeahorseSource* _source;
	SeahorseObject* _preferred;
	SeahorseObject* _parent;
	GList* _children;
};

#define SEAHORSE_OBJECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_OBJECT, SeahorseObjectPrivate))
enum  {
	SEAHORSE_OBJECT_DUMMY_PROPERTY,
	SEAHORSE_OBJECT_TAG,
	SEAHORSE_OBJECT_ID,
	SEAHORSE_OBJECT_LOCATION,
	SEAHORSE_OBJECT_USAGE,
	SEAHORSE_OBJECT_FLAGS,
	SEAHORSE_OBJECT_SOURCE,
	SEAHORSE_OBJECT_PREFERRED,
	SEAHORSE_OBJECT_DESCRIPTION,
	SEAHORSE_OBJECT_MARKUP,
	SEAHORSE_OBJECT_STOCK_ICON,
	SEAHORSE_OBJECT_PARENT
};
static void seahorse_object_register_child (SeahorseObject* self, SeahorseObject* child);
static void seahorse_object_unregister_child (SeahorseObject* self, SeahorseObject* child);
static GObject * seahorse_object_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_object_parent_class = NULL;
static void seahorse_object_dispose (GObject * obj);




GType seahorse_object_change_get_type (void) {
	static GType seahorse_object_change_type_id = 0;
	if (G_UNLIKELY (seahorse_object_change_type_id == 0)) {
		static const GEnumValue values[] = {{SEAHORSE_OBJECT_CHANGE_ALL, "SEAHORSE_OBJECT_CHANGE_ALL", "all"}, {SEAHORSE_OBJECT_CHANGE_LOCATION, "SEAHORSE_OBJECT_CHANGE_LOCATION", "location"}, {SEAHORSE_OBJECT_CHANGE_PREFERRED, "SEAHORSE_OBJECT_CHANGE_PREFERRED", "preferred"}, {SEAHORSE_OBJECT_CHANGE_MAX, "SEAHORSE_OBJECT_CHANGE_MAX", "max"}, {0, NULL, NULL}};
		seahorse_object_change_type_id = g_enum_register_static ("SeahorseObjectChange", values);
	}
	return seahorse_object_change_type_id;
}


GList* seahorse_object_get_children (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return g_list_copy (self->priv->_children);
}


void seahorse_object_fire_changed (SeahorseObject* self, SeahorseObjectChange what) {
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	g_signal_emit_by_name (G_OBJECT (self), "changed", what);
}


static void seahorse_object_register_child (SeahorseObject* self, SeahorseObject* child) {
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	g_return_if_fail (SEAHORSE_IS_OBJECT (child));
	g_assert (child->priv->_parent == NULL);
	child->priv->_parent = self;
	self->priv->_children = g_list_append (self->priv->_children, child);
}


static void seahorse_object_unregister_child (SeahorseObject* self, SeahorseObject* child) {
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	g_return_if_fail (SEAHORSE_IS_OBJECT (child));
	g_assert (child->priv->_parent == self);
	child->priv->_parent = NULL;
	self->priv->_children = g_list_remove (self->priv->_children, child);
}


GQuark seahorse_object_get_tag (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0U);
	return self->_tag;
}


GQuark seahorse_object_get_id (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0U);
	return self->_id;
}


SeahorseLocation seahorse_object_get_location (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0);
	return self->_location;
}


void seahorse_object_set_location (SeahorseObject* self, SeahorseLocation value) {
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	self->_location = value;
	seahorse_object_fire_changed (self, SEAHORSE_OBJECT_CHANGE_LOCATION);
}


SeahorseUsage seahorse_object_get_usage (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0);
	return self->_usage;
}


guint seahorse_object_get_flags (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0U);
	return self->_flags;
}


SeahorseSource* seahorse_object_get_source (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->priv->_source;
}


void seahorse_object_set_source (SeahorseObject* self, SeahorseSource* value) {
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	if (self->priv->_source != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (self->priv->_source), &self->priv->_source);
	}
	self->priv->_source = value;
	if (self->priv->_source != NULL) {
		g_object_add_weak_pointer (G_OBJECT (self->priv->_source), &self->priv->_source);
	}
}


SeahorseObject* seahorse_object_get_preferred (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->priv->_preferred;
}


void seahorse_object_set_preferred (SeahorseObject* self, SeahorseObject* value) {
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	if (self->priv->_preferred == value) {
		return;
	}
	if (self->priv->_preferred != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (self->priv->_preferred), &self->priv->_preferred);
	}
	self->priv->_preferred = value;
	if (self->priv->_preferred != NULL) {
		g_object_add_weak_pointer (G_OBJECT (self->priv->_preferred), &self->priv->_preferred);
	}
	seahorse_object_fire_changed (self, SEAHORSE_OBJECT_CHANGE_PREFERRED);
}


char* seahorse_object_get_description (SeahorseObject* self) {
	char* value;
	g_object_get (G_OBJECT (self), "description", &value, NULL);
	return value;
}


char* seahorse_object_get_markup (SeahorseObject* self) {
	char* value;
	g_object_get (G_OBJECT (self), "markup", &value, NULL);
	return value;
}


char* seahorse_object_get_stock_icon (SeahorseObject* self) {
	char* value;
	g_object_get (G_OBJECT (self), "stock-icon", &value, NULL);
	return value;
}


SeahorseObject* seahorse_object_get_parent (SeahorseObject* self) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->priv->_parent;
}


void seahorse_object_set_parent (SeahorseObject* self, SeahorseObject* value) {
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	/* Set the new parent/child relationship */
	if (self->priv->_parent != NULL) {
		seahorse_object_unregister_child (self->priv->_parent, self);
	}
	if (value != NULL) {
		seahorse_object_register_child (value, self);
	}
	g_assert (self->priv->_parent == value);
	/* Fire a signal to let watchers know this changed */
	g_signal_emit_by_name (G_OBJECT (self), "hierarchy");
}


static GObject * seahorse_object_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorseObjectClass * klass;
	GObjectClass * parent_class;
	SeahorseObject * self;
	klass = SEAHORSE_OBJECT_CLASS (g_type_class_peek (SEAHORSE_TYPE_OBJECT));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_OBJECT (obj);
	{
		self->priv->_parent = NULL;
		self->priv->_children = NULL;
	}
	return obj;
}


gboolean seahorse_object_predicate_match (SeahorseObjectPredicate *self, SeahorseObject* obj) {
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (obj), FALSE);
	/* Check all the fields */
	if ((*self).tag != 0 && ((*self).tag != obj->_tag)) {
		return FALSE;
	}
	if ((*self).id != 0 && ((*self).id != obj->_id)) {
		return FALSE;
	}
	if ((*self).location != 0 && ((*self).location != obj->_location)) {
		return FALSE;
	}
	if ((*self).usage != 0 && ((*self).usage != obj->_usage)) {
		return FALSE;
	}
	if ((*self).flags != 0 && ((*self).flags & obj->_flags) == 0) {
		return FALSE;
	}
	if ((*self).nflags != 0 && ((*self).nflags & obj->_flags) != 0) {
		return FALSE;
	}
	if ((*self).source != NULL && ((*self).source != obj->priv->_source)) {
		return FALSE;
	}
	/* And any custom stuff */
	if ((*self).custom != NULL && !(*self).custom (obj, (*self).custom_target)) {
		return FALSE;
	}
	return TRUE;
}


static void seahorse_object_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorseObject * self;
	self = SEAHORSE_OBJECT (object);
	switch (property_id) {
		case SEAHORSE_OBJECT_TAG:
		g_value_set_uint (value, seahorse_object_get_tag (self));
		break;
		case SEAHORSE_OBJECT_ID:
		g_value_set_uint (value, seahorse_object_get_id (self));
		break;
		case SEAHORSE_OBJECT_LOCATION:
		g_value_set_enum (value, seahorse_object_get_location (self));
		break;
		case SEAHORSE_OBJECT_USAGE:
		g_value_set_enum (value, seahorse_object_get_usage (self));
		break;
		case SEAHORSE_OBJECT_FLAGS:
		g_value_set_uint (value, seahorse_object_get_flags (self));
		break;
		case SEAHORSE_OBJECT_SOURCE:
		g_value_set_object (value, seahorse_object_get_source (self));
		break;
		case SEAHORSE_OBJECT_PREFERRED:
		g_value_set_object (value, seahorse_object_get_preferred (self));
		break;
		case SEAHORSE_OBJECT_PARENT:
		g_value_set_object (value, seahorse_object_get_parent (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_object_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorseObject * self;
	self = SEAHORSE_OBJECT (object);
	switch (property_id) {
		case SEAHORSE_OBJECT_LOCATION:
		seahorse_object_set_location (self, g_value_get_enum (value));
		break;
		case SEAHORSE_OBJECT_SOURCE:
		seahorse_object_set_source (self, g_value_get_object (value));
		break;
		case SEAHORSE_OBJECT_PREFERRED:
		seahorse_object_set_preferred (self, g_value_get_object (value));
		break;
		case SEAHORSE_OBJECT_PARENT:
		seahorse_object_set_parent (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_object_class_init (SeahorseObjectClass * klass) {
	seahorse_object_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseObjectPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_object_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_object_set_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_object_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_object_dispose;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_TAG, g_param_spec_uint ("tag", "tag", "tag", 0, G_MAXUINT, 0U, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_ID, g_param_spec_uint ("id", "id", "id", 0, G_MAXUINT, 0U, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_LOCATION, g_param_spec_enum ("location", "location", "location", SEAHORSE_TYPE_LOCATION, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_USAGE, g_param_spec_enum ("usage", "usage", "usage", SEAHORSE_TYPE_USAGE, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_FLAGS, g_param_spec_uint ("flags", "flags", "flags", 0, G_MAXUINT, 0U, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_SOURCE, g_param_spec_object ("source", "source", "source", SEAHORSE_TYPE_SOURCE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_PREFERRED, g_param_spec_object ("preferred", "preferred", "preferred", SEAHORSE_TYPE_OBJECT, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_DESCRIPTION, g_param_spec_string ("description", "description", "description", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_MARKUP, g_param_spec_string ("markup", "markup", "markup", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_STOCK_ICON, g_param_spec_string ("stock-icon", "stock-icon", "stock-icon", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_OBJECT_PARENT, g_param_spec_object ("parent", "parent", "parent", SEAHORSE_TYPE_OBJECT, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_signal_new ("changed", SEAHORSE_TYPE_OBJECT, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__ENUM, G_TYPE_NONE, 1, SEAHORSE_OBJECT_TYPE_CHANGE);
	g_signal_new ("hierarchy", SEAHORSE_TYPE_OBJECT, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	g_signal_new ("destroy", SEAHORSE_TYPE_OBJECT, G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}


static void seahorse_object_instance_init (SeahorseObject * self) {
	self->priv = SEAHORSE_OBJECT_GET_PRIVATE (self);
}


static void seahorse_object_dispose (GObject * obj) {
	SeahorseObject * self;
	self = SEAHORSE_OBJECT (obj);
	{
		SeahorseObject* _tmp0;
		SeahorseObject* new_parent;
		if (self->priv->_source != NULL) {
			g_object_remove_weak_pointer (G_OBJECT (self->priv->_source), &self->priv->_source);
		}
		if (self->priv->_preferred != NULL) {
			g_object_remove_weak_pointer (G_OBJECT (self->priv->_source), &self->priv->_preferred);
		}
		/* 
		 * When an object is destroyed, we reparent all
		 * children to this objects parent. If no parent
		 * of this object, all children become root objects.
		 */
		_tmp0 = NULL;
		new_parent = (_tmp0 = self->priv->_parent, (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
		{
			GList* child_collection;
			GList* child_it;
			child_collection = self->priv->_children;
			for (child_it = child_collection; child_it != NULL; child_it = child_it->next) {
				SeahorseObject* _tmp1;
				SeahorseObject* child;
				_tmp1 = NULL;
				child = (_tmp1 = ((SeahorseObject*) (child_it->data)), (_tmp1 == NULL ? NULL : g_object_ref (_tmp1)));
				{
					seahorse_object_unregister_child (self, child);
					if (new_parent != NULL) {
						seahorse_object_register_child (new_parent, child);
					}
					/* Fire signal to let anyone know this has moved */
					g_signal_emit_by_name (G_OBJECT (child), "hierarchy");
					(child == NULL ? NULL : (child = (g_object_unref (child), NULL)));
				}
			}
		}
		/* Fire our destroy signal, so that any watchers can go away */
		g_signal_emit_by_name (G_OBJECT (self), "destroy");
		(new_parent == NULL ? NULL : (new_parent = (g_object_unref (new_parent), NULL)));
	}
	(self->attached_to == NULL ? NULL : (self->attached_to = (g_object_unref (self->attached_to), NULL)));
	G_OBJECT_CLASS (seahorse_object_parent_class)->dispose (obj);
}


GType seahorse_object_get_type (void) {
	static GType seahorse_object_type_id = 0;
	if (G_UNLIKELY (seahorse_object_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseObjectClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_object_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseObject), 0, (GInstanceInitFunc) seahorse_object_instance_init };
		seahorse_object_type_id = g_type_register_static (G_TYPE_OBJECT, "SeahorseObject", &g_define_type_info, G_TYPE_FLAG_ABSTRACT);
	}
	return seahorse_object_type_id;
}




