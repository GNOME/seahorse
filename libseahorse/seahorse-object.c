/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "config.h"

#include "seahorse-object.h"
#include "seahorse-place.h"
#include "seahorse-predicate.h"

#include "string.h"

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_PLACE,
	PROP_ACTIONS,
	PROP_LABEL,
	PROP_MARKUP,
	PROP_NICKNAME,
	PROP_ICON,
	PROP_IDENTIFIER,
	PROP_USAGE,
	PROP_FLAGS,
	PROP_DELETABLE,
	PROP_EXPORTABLE
};

/**
 * @place: Where did the object come from ?
 * @label: DBUS: "display-name"
 * @markup: Markup text
 * @markup_explicit: If TRUE the markup will not be set automatically
 * @nickname: DBUS: "simple-name"
 * @nickname_explicit: If TRUE the nickname will not be set automatically
 * @icon: GIcon
 * @identifier: DBUS: "key-id", "display-id", "raw-id"
 * @identifier_explicit:
 * @location: describes the loaction of the object (local, remte, invalid...)
 * @usage: DBUS: "etype"
 * @flags:
 */
struct _SeahorseObjectPrivate {
	SeahorsePlace *place;
	GtkActionGroup *actions;

    gchar *label;             
    gchar *markup;
    gboolean markup_explicit;
    gchar *nickname;
    gboolean nickname_explicit;
    GIcon *icon;
    gchar *identifier;
    gboolean identifier_explicit;

    SeahorseUsage usage;
    guint flags;
};

G_DEFINE_TYPE (SeahorseObject, seahorse_object, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/**
 * set_string_storage:
 * @value: The value to store
 * @storage: The datastore to write the value to
 *
 * When storing, new memory will be allocated and the value copied
 *
 * Returns: TRUE if the value has been set new, FALSE else
 */
static gboolean
set_string_storage (const gchar *value, gchar **storage)
{
	g_assert (storage);

	if (value == NULL)
		value = "";
	
	if (!value || !*storage || !g_str_equal (value, *storage)) {
		g_free (*storage);
		*storage = g_strdup (value);
		return TRUE;
	}
	
	return FALSE;
}

/**
 * take_string_storage:
 * @value: The value to store
 * @storage: The datastore to write the value to
 *
 * When storing, the pointer to value is used. No new memory will be allocated
 *
 * Returns: TRUE if the value has been set new, FALSE else
 */
static gboolean
take_string_storage (gchar *value, gchar **storage)
{
	g_assert (storage);
	
	if (value == NULL)
		value = g_strdup ("");
	
	if (!value || !*storage || !g_str_equal (value, *storage)) {
		g_free (*storage);
		*storage = value;
		return TRUE;
	}
	
	g_free (value);
	return FALSE;
}

/**
 * recalculate_label:
 * @self: object to recalculate label for
 *
 * Recalculates nickname and markup from the label
 *
 */
static void
recalculate_label (SeahorseObject *self)
{
	if (!self->pv->markup_explicit) {
		if (take_string_storage (g_markup_escape_text (self->pv->label ? self->pv->label : "", -1),
		                         &self->pv->markup))
			g_object_notify (G_OBJECT (self), "markup");
	}

	if (!self->pv->nickname_explicit) {
		if (set_string_storage (self->pv->label, &self->pv->nickname))
			g_object_notify (G_OBJECT (self), "nickname");
	}
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

/**
 * seahorse_object_init:
 * @self: The object to initialise
 *
 * Initialises the object with default data
 *
 */
static void
seahorse_object_init (SeahorseObject *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_OBJECT, 
	                                        SeahorseObjectPrivate);
	self->pv->label = g_strdup ("");
	self->pv->markup = g_strdup ("");
	self->pv->icon = g_themed_icon_new ("gtk-missing-image");
	self->pv->identifier = g_strdup ("");
	self->pv->usage = SEAHORSE_USAGE_NONE;
}


/**
 * seahorse_object_dispose:
 * @obj: A #SeahorseObject to dispose
 *
 *
 */
static void
seahorse_object_dispose (GObject *obj)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);

	if (self->pv->place != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (self->pv->place),
		                              (gpointer*)&self->pv->place);
		self->pv->place = NULL;
	}

	G_OBJECT_CLASS (seahorse_object_parent_class)->dispose (obj);	
}

/**
 * seahorse_object_finalize:
 * @obj: #SeahorseObject to finalize
 *
 */
static void
seahorse_object_finalize (GObject *obj)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);

	g_assert (self->pv->place == NULL);
	g_clear_object (&self->pv->actions);

	g_free (self->pv->label);
	self->pv->label = NULL;
	
	g_free (self->pv->markup);
	self->pv->markup = NULL;
	
	g_free (self->pv->nickname);
	self->pv->nickname = NULL;

	g_clear_object (&self->pv->icon);

	g_free (self->pv->identifier);
	self->pv->identifier = NULL;

	G_OBJECT_CLASS (seahorse_object_parent_class)->finalize (obj);
}


/**
 * seahorse_object_get_property:
 * @obj: The object to get the property for
 * @prop_id: The property requested
 * @value: out - the value as #GValue
 * @pspec: a #GParamSpec for the warning
 *
 * Returns: The property of the object @obj defined by the id @prop_id in @value.
 *
 */
static void
seahorse_object_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);

	switch (prop_id) {
	case PROP_PLACE:
		g_value_set_object (value, seahorse_object_get_place (self));
		break;
	case PROP_ACTIONS:
		g_value_set_object (value, self->pv->actions);
		break;
	case PROP_LABEL:
		g_value_set_string (value, seahorse_object_get_label (self));
		break;
	case PROP_NICKNAME:
		g_value_set_string (value, seahorse_object_get_nickname (self));
		break;
	case PROP_MARKUP:
		g_value_set_string (value, seahorse_object_get_markup (self));
		break;
	case PROP_ICON:
		g_value_set_object (value, self->pv->icon);
		break;
	case PROP_IDENTIFIER:
		g_value_set_string (value, seahorse_object_get_identifier (self));
		break;
	case PROP_USAGE:
		g_value_set_enum (value, seahorse_object_get_usage (self));
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, seahorse_object_get_flags (self));
		break;
	case PROP_DELETABLE:
		g_value_set_boolean (value, seahorse_object_get_flags (self) & SEAHORSE_FLAG_DELETABLE ? TRUE : FALSE);
		break;
	case PROP_EXPORTABLE:
		g_value_set_boolean (value, seahorse_object_get_flags (self) & SEAHORSE_FLAG_EXPORTABLE ? TRUE : FALSE);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

/**
 * seahorse_object_set_property:
 * @obj: Object to set the property in
 * @prop_id: the property to set
 * @value: the value to set
 * @pspec: To be used for warnings. #GParamSpec
 *
 * Sets a property in this object
 *
 */
static void
seahorse_object_set_property (GObject *obj, guint prop_id, const GValue *value, 
                              GParamSpec *pspec)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);
	SeahorseUsage usage;
	guint flags;

	switch (prop_id) {
	case PROP_PLACE:
		seahorse_object_set_place (self, g_value_get_object (value));
		break;
	case PROP_ACTIONS:
		g_clear_object (&self->pv->actions);
		self->pv->actions = g_value_dup_object (value);
		break;
	case PROP_LABEL:
		if (set_string_storage (g_value_get_string (value), &self->pv->label)) {
			g_object_freeze_notify (obj);
			g_object_notify (obj, "label");
			recalculate_label (self);
			g_object_thaw_notify (obj);
		}
		break;
	case PROP_NICKNAME:
		if (set_string_storage (g_value_get_string (value), &self->pv->nickname)) {
			self->pv->nickname_explicit = TRUE;
			g_object_notify (obj, "nickname");
		}
		break;
	case PROP_MARKUP:
		if (set_string_storage (g_value_get_string (value), &self->pv->markup)) {
			self->pv->markup_explicit = TRUE;
			g_object_notify (obj, "markup");
		}
		break;
	case PROP_ICON:
		g_clear_object (&self->pv->icon);
		self->pv->icon = g_value_dup_object (value);
		g_object_notify (obj, "icon");
		break;
	case PROP_IDENTIFIER:
		if (set_string_storage (g_value_get_string (value), &self->pv->identifier)) {
			self->pv->identifier_explicit = TRUE;
			g_object_notify (obj, "identifier");
		}
		break;
	case PROP_USAGE:
		usage = g_value_get_enum (value);
		if (usage != self->pv->usage) {
			self->pv->usage = usage;
			g_object_freeze_notify (obj);
			g_object_notify (obj, "usage");
			g_object_thaw_notify (obj);
		}
		break;
	case PROP_FLAGS:
		flags = g_value_get_uint (value);
		if (flags != self->pv->flags) {
			self->pv->flags = flags;
			g_object_notify (obj, "object-flags");
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

/**
 * seahorse_object_class_init:
 * @klass: the object class
 *
 * Initialises the object class
 *
 */
static void
seahorse_object_class_init (SeahorseObjectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_object_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseObjectPrivate));

	gobject_class->dispose = seahorse_object_dispose;
	gobject_class->finalize = seahorse_object_finalize;
	gobject_class->set_property = seahorse_object_set_property;
	gobject_class->get_property = seahorse_object_get_property;

	g_object_class_install_property (gobject_class, PROP_PLACE,
	           g_param_spec_object ("place", "Object Place", "Place the Object came from",
	                                SEAHORSE_TYPE_PLACE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_ACTIONS,
	           g_param_spec_object ("actions", "Actions", "Actions for the object",
	                                GTK_TYPE_ACTION_GROUP, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_LABEL,
	           g_param_spec_string ("label", "Object Display Label", "This object's displayable label.", 
	                                "", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_NICKNAME,
	           g_param_spec_string ("nickname", "Object Short Name", "This object's short name.", 
	                                "", G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_ICON,
	           g_param_spec_object ("icon", "Object Icon", "Stock ID for object.",
	                                G_TYPE_ICON, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_MARKUP,
	           g_param_spec_string ("markup", "Object Display Markup", "This object's displayable markup.", 
	                                "", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_IDENTIFIER,
	           g_param_spec_string ("identifier", "Object Identifier", "Displayable ID for the object.", 
	                                "", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_USAGE,
	           g_param_spec_enum ("usage", "Object Usage", "How this object is used.", 
	                              SEAHORSE_TYPE_USAGE, SEAHORSE_USAGE_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	           g_param_spec_uint ("object-flags", "Object Flags", "This object's flags.",
	                              0, G_MAXUINT, 0, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_DELETABLE,
	           g_param_spec_boolean ("deletable", "Deletable", "Object is deletable.",
	                                 FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_EXPORTABLE,
	           g_param_spec_boolean ("exportable", "Exportable", "Object is exportable.",
	                                 FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

/**
 * seahorse_object_get_source:
 * @self: Object
 *
 * Returns: the source of the object @self
 */
SeahorsePlace *
seahorse_object_get_place (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->place;
}


/**
 * seahorse_object_set_source:
 * @self: The object to set a new source for
 * @value: The source to set
 *
 * sets the source for the object
 */
void
seahorse_object_set_place (SeahorseObject *self,
                           SeahorsePlace *value)
{
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));

	if (value == self->pv->place)
		return;

	if (self->pv->place)
		g_object_remove_weak_pointer (G_OBJECT (self->pv->place),
		                              (gpointer*)&self->pv->place);

	self->pv->place = value;
	if (self->pv->place != NULL)
		g_object_add_weak_pointer (G_OBJECT (self->pv->place),
		                           (gpointer*)&self->pv->place);

	g_object_notify (G_OBJECT (self), "place");
}

/**
 * seahorse_object_get_label:
 * @self: Object
 *
 * Returns: the label of the object @self
 */
const gchar*
seahorse_object_get_label (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->label;
}

/**
 * seahorse_object_get_markup:
 * @self: Object
 *
 * Returns: the markup of the object @self
 */
const gchar*
seahorse_object_get_markup (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->markup;
}

/**
 * seahorse_object_get_nickname:
 * @self: Object
 *
 * Returns: the nickname of the object @self
 */
const gchar*
seahorse_object_get_nickname (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->nickname;
}

/**
 * seahorse_object_get_identifier:
 * @self: Object
 *
 * Returns: the identifier of the object @self
 */
const gchar*
seahorse_object_get_identifier (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->identifier;
}

/**
 * seahorse_object_get_usage:
 * @self: Object
 *
 * Returns: the usage of the object @self
 */
SeahorseUsage
seahorse_object_get_usage (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), SEAHORSE_USAGE_NONE);
	return self->pv->usage;	
}

/**
 * seahorse_object_get_flags:
 * @self: Object
 *
 * Returns: the flags of the object @self
 */
guint
seahorse_object_get_flags (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0);
	return self->pv->flags;	
}
