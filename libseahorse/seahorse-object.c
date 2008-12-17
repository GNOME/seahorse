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

#include "config.h"

#include "seahorse-object.h"

#include "seahorse-context.h"
#include "seahorse-source.h"

#include "string.h"

#include <glib/gi18n.h>

enum {
	PROP_0,
	PROP_CONTEXT,
	PROP_SOURCE,
	PROP_PREFERRED,
	PROP_PARENT,
	PROP_ID,
	PROP_TAG,
	PROP_LABEL,
	PROP_MARKUP,
	PROP_NICKNAME,
	PROP_DESCRIPTION,
	PROP_ICON,
	PROP_IDENTIFIER,
	PROP_LOCATION,
	PROP_USAGE,
	PROP_FLAGS
};

struct _SeahorseObjectPrivate {
	GQuark id;
	GQuark tag;
	gboolean tag_explicit;

	SeahorseSource *source;
	SeahorseContext *context;
	SeahorseObject *preferred;
	SeahorseObject *parent;
	GList *children;
	
	gchar *label;
	gchar *markup;
	gboolean markup_explicit;
	gchar *nickname;
	gboolean nickname_explicit;
	gchar *description;
	gboolean description_explicit;
	gchar *icon;
	gchar *identifier;
	gboolean identifier_explicit;
	
	SeahorseLocation location;
	SeahorseUsage usage;
	guint flags;
	
	gboolean realized;
	gboolean realizing;
};

G_DEFINE_TYPE (SeahorseObject, seahorse_object, G_TYPE_OBJECT);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void 
register_child (SeahorseObject* self, SeahorseObject* child) 
{
	g_assert (SEAHORSE_IS_OBJECT (self));
	g_assert (SEAHORSE_IS_OBJECT (child));
	g_assert (self != child);
	g_assert (child->pv->parent == NULL);
	
	child->pv->parent = self;
	self->pv->children = g_list_append (self->pv->children, child);
}

static void 
unregister_child (SeahorseObject* self, SeahorseObject* child) 
{
	g_assert (SEAHORSE_IS_OBJECT (self));
	g_assert (SEAHORSE_IS_OBJECT (child));
	g_assert (self != child);
	g_assert (child->pv->parent == self);
	
	child->pv->parent = NULL;
	self->pv->children = g_list_remove (self->pv->children, child);
}

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

static void
recalculate_id (SeahorseObject *self)
{
	const gchar *str;
	const gchar *at;
	GQuark tag;
	gchar *result;
	gsize len;

	/* No id, clear any tag and auto generated identifer */
	if (!self->pv->id) {
		if (!self->pv->tag_explicit) {
			self->pv->tag = 0;
			g_object_notify (G_OBJECT (self), "tag");
		}
			
		if (!self->pv->identifier_explicit) {
			if (set_string_storage ("", &self->pv->identifier))
				g_object_notify (G_OBJECT (self), "identifier");
		}
		
	/* Have hande, configure tag and auto generated identifer */
	} else {
		str = g_quark_to_string (self->pv->id);
		len = strlen (str);
		at = strchr (str, ':');
		
		if (!self->pv->tag_explicit) {
			result = g_strndup (str, at ? at - str : len);
			tag = g_quark_from_string (result);
			g_free (result);
			
			if (tag != self->pv->tag) {
				self->pv->tag = tag;
				g_object_notify (G_OBJECT (self), "tag");
			}
		}
		
		if (!self->pv->identifier_explicit) {
			if (set_string_storage (at ? at + 1 : "", &self->pv->identifier))
				g_object_notify (G_OBJECT (self), "identifier");
		}
	}
}

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

static void
recalculate_usage (SeahorseObject *self)
{
	const gchar *desc;
	
	if (!self->pv->description_explicit) {
		
		switch (self->pv->usage) {
		case SEAHORSE_USAGE_SYMMETRIC_KEY:
			desc = _("Symmetric Key");
			break;
		case SEAHORSE_USAGE_PUBLIC_KEY:
			desc = _("Public Key");
			break;
		case SEAHORSE_USAGE_PRIVATE_KEY:
			desc = _("Private Key");
			break;
		case SEAHORSE_USAGE_CREDENTIALS:
			desc = _("Credentials");
			break;
		case SEAHORSE_USAGE_IDENTITY:
			desc = _("Identity");
			break;
		default:
			desc = "";
			break;
		}
		
		if (set_string_storage (desc, &self->pv->description))
			g_object_notify (G_OBJECT (self), "description");
	}
}


/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_object_init (SeahorseObject *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_OBJECT, 
	                                        SeahorseObjectPrivate);
	self->pv->label = g_strdup ("");
	self->pv->markup = g_strdup ("");
	self->pv->description = g_strdup ("");
	self->pv->icon = g_strdup ("gtk-missing-image");
	self->pv->identifier = g_strdup ("");
	self->pv->location = SEAHORSE_LOCATION_INVALID;
	self->pv->usage = SEAHORSE_USAGE_NONE;
}

static void
seahorse_object_dispose (GObject *obj)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);
	SeahorseObject *parent;
	GList *l;
	
	if (self->pv->context != NULL) {
		seahorse_context_remove_object (self->pv->context, self);
		g_assert (self->pv->context == NULL);
	}
	
	if (self->pv->source != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (self->pv->source), (gpointer*)&self->pv->source);
		self->pv->source = NULL;
	}
	
	if (self->pv->preferred != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (self->pv->source), (gpointer*)&self->pv->preferred);
		self->pv->preferred = NULL;
	}

	/* 
	 * When an object is destroyed, we reparent all
	 * children to this objects parent. If no parent
	 * of this object, all children become root objects.
	 */

	parent = self->pv->parent;
	if (parent)
		g_object_ref (parent);
	
	for (l = self->pv->children; l; l = g_list_next (l)) {
		g_return_if_fail (SEAHORSE_IS_OBJECT (l->data));
		seahorse_object_set_parent (l->data, parent);
	}

	if (parent)
		g_object_unref (parent);
	
	g_assert (self->pv->children == NULL);
	
	/* Now remove this object from its parent */
	seahorse_object_set_parent (self, NULL);
	
	G_OBJECT_CLASS (seahorse_object_parent_class)->dispose (obj);	
}

static void
seahorse_object_finalize (GObject *obj)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);
	
	g_assert (self->pv->source == NULL);
	g_assert (self->pv->preferred == NULL);
	g_assert (self->pv->parent == NULL);
	g_assert (self->pv->context == NULL);
	g_assert (self->pv->children == NULL);
	
	g_free (self->pv->label);
	self->pv->label = NULL;
	
	g_free (self->pv->markup);
	self->pv->markup = NULL;
	
	g_free (self->pv->nickname);
	self->pv->nickname = NULL;
	
	g_free (self->pv->description);
	self->pv->description = NULL;
	
	g_free (self->pv->icon);
	self->pv->icon = NULL;
	
	g_free (self->pv->identifier);
	self->pv->identifier = NULL;

	G_OBJECT_CLASS (seahorse_object_parent_class)->finalize (obj);
}

static void
seahorse_object_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);
	
	switch (prop_id) {
	case PROP_CONTEXT:
		g_value_set_object (value, seahorse_object_get_context (self));
		break;
	case PROP_SOURCE:
		g_value_set_object (value, seahorse_object_get_source (self));
		break;
	case PROP_PREFERRED:
		g_value_set_object (value, seahorse_object_get_preferred (self));
		break;
	case PROP_PARENT:
		g_value_set_object (value, seahorse_object_get_parent (self));
		break;
	case PROP_ID:
		g_value_set_uint (value, seahorse_object_get_id (self));
		break;
	case PROP_TAG:
		g_value_set_uint (value, seahorse_object_get_tag (self));
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
	case PROP_DESCRIPTION:
		g_value_set_string (value, seahorse_object_get_description (self));
		break;
	case PROP_ICON:
		g_value_set_string (value, seahorse_object_get_icon (self));
		break;
	case PROP_IDENTIFIER:
		g_value_set_string (value, seahorse_object_get_identifier (self));
		break;
	case PROP_LOCATION:
		g_value_set_enum (value, seahorse_object_get_location (self));
		break;
	case PROP_USAGE:
		g_value_set_enum (value, seahorse_object_get_usage (self));
		break;
	case PROP_FLAGS:
		g_value_set_uint (value, seahorse_object_get_flags (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_object_set_property (GObject *obj, guint prop_id, const GValue *value, 
                              GParamSpec *pspec)
{
	SeahorseObject *self = SEAHORSE_OBJECT (obj);
	SeahorseLocation loc;
	SeahorseUsage usage;
	guint flags;
	GQuark quark;
	
	switch (prop_id) {
	case PROP_CONTEXT:
		if (self->pv->context) 
			g_object_remove_weak_pointer (G_OBJECT (self->pv->context), (gpointer*)&self->pv->context);
		self->pv->context = SEAHORSE_CONTEXT (g_value_get_object (value));
		if (self->pv->context != NULL)
			g_object_add_weak_pointer (G_OBJECT (self->pv->context), (gpointer*)&self->pv->context);
		g_object_notify (G_OBJECT (self), "context");
		break;
	case PROP_SOURCE:
		seahorse_object_set_source (self, SEAHORSE_SOURCE (g_value_get_object (value)));
		break;
	case PROP_PREFERRED:
		seahorse_object_set_preferred (self, SEAHORSE_OBJECT (g_value_get_object (value)));
		break;
	case PROP_PARENT:
		seahorse_object_set_parent (self, SEAHORSE_OBJECT (g_value_get_object (value)));
		break;
	case PROP_ID:
		quark = g_value_get_uint (value);
		if (quark != self->pv->id) {
			self->pv->id = quark;
			g_object_freeze_notify (obj);
			g_object_notify (obj, "id");
			recalculate_id (self);
			g_object_thaw_notify (obj);
		}
		break;
	case PROP_TAG:
		quark = g_value_get_uint (value);
		if (quark != self->pv->tag) {
			self->pv->tag = quark;
			self->pv->tag_explicit = TRUE;
			g_object_notify (obj, "tag");
		}
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
	case PROP_DESCRIPTION:
		if (set_string_storage (g_value_get_string (value), &self->pv->description)) {
			self->pv->description_explicit = TRUE;
			g_object_notify (obj, "description");
		}
		break;
	case PROP_ICON:
		if (set_string_storage (g_value_get_string (value), &self->pv->icon))
			g_object_notify (obj, "icon");
		break;
	case PROP_IDENTIFIER:
		if (set_string_storage (g_value_get_string (value), &self->pv->identifier)) {
			self->pv->identifier_explicit = TRUE;
			g_object_notify (obj, "identifier");
		}
		break;
	case PROP_LOCATION:
		loc = g_value_get_enum (value);
		if (loc != self->pv->location) {
			self->pv->location = loc;
			g_object_notify (obj, "location");
		}
		break;
	case PROP_USAGE:
		usage = g_value_get_enum (value);
		if (usage != self->pv->usage) {
			self->pv->usage = usage;
			g_object_freeze_notify (obj);
			g_object_notify (obj, "usage");
			recalculate_usage (self);
			g_object_thaw_notify (obj);
		}
		break;
	case PROP_FLAGS:
		flags = g_value_get_uint (value);
		if (SEAHORSE_OBJECT_GET_CLASS (obj)->delete)
			flags |= SEAHORSE_FLAG_DELETABLE;
		else 
			flags &= ~SEAHORSE_FLAG_DELETABLE;
		if (flags != self->pv->flags) {
			self->pv->flags = flags;
			g_object_notify (obj, "flags");
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void 
seahorse_object_real_realize (SeahorseObject *self)
{
	/* 
	 * We do nothing by default. It's up to the derived class
	 * to override this and realize themselves when called.
	 */
	
	self->pv->realized = TRUE;
}

static void 
seahorse_object_real_refresh (SeahorseObject *self)
{
	/* 
	 * We do nothing by default. It's up to the derived class
	 * to override this and refresh themselves when called.
	 */
}

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
	
	klass->realize = seahorse_object_real_realize;
	klass->refresh = seahorse_object_real_refresh;
	
	g_object_class_install_property (gobject_class, PROP_SOURCE,
	           g_param_spec_object ("source", "Object Source", "Source the Object came from", 
	                                SEAHORSE_TYPE_SOURCE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CONTEXT,
	           g_param_spec_object ("context", "Context for object", "Object that this context belongs to", 
	                                SEAHORSE_TYPE_CONTEXT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_PREFERRED,
	           g_param_spec_object ("preferred", "Preferred Object", "An object to prefer over this one", 
	                                SEAHORSE_TYPE_OBJECT, G_PARAM_READWRITE));
    
	g_object_class_install_property (gobject_class, PROP_PREFERRED,
	           g_param_spec_object ("parent", "Parent Object", "This object's parent in the tree.", 
	                                SEAHORSE_TYPE_OBJECT, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_ID,
	           g_param_spec_uint ("id", "Object ID", "This object's ID.", 
	                              0, G_MAXUINT, 0, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_TAG,
	           g_param_spec_uint ("tag", "Object Type Tag", "This object's type tag.", 
	                              0, G_MAXUINT, 0, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_LABEL,
	           g_param_spec_string ("label", "Object Display Label", "This object's displayable label.", 
	                                "", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_NICKNAME,
	           g_param_spec_string ("nickname", "Object Short Name", "This object's short name.", 
	                                "", G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_ICON,
	           g_param_spec_string ("icon", "Object Icon", "Stock ID for object.", 
	                                "", G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_MARKUP,
	           g_param_spec_string ("markup", "Object Display Markup", "This object's displayable markup.", 
	                                "", G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
	           g_param_spec_string ("description", "Object Description", "Description of the type of object", 
	                                "", G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_IDENTIFIER,
	           g_param_spec_string ("identifier", "Object Identifier", "Displayable ID for the object.", 
	                                "", G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_LOCATION,
	           g_param_spec_enum ("location", "Object Location", "Where the object is located.", 
	                              SEAHORSE_TYPE_LOCATION, SEAHORSE_LOCATION_INVALID, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_USAGE,
	           g_param_spec_enum ("usage", "Object Usage", "How this object is used.", 
	                              SEAHORSE_TYPE_USAGE, SEAHORSE_USAGE_NONE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_FLAGS,
	           g_param_spec_uint ("flags", "Object Flags", "This object's flags.", 
	                              0, G_MAXUINT, 0, G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

GQuark
seahorse_object_get_id (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0);
	if (!self->pv->id)
		seahorse_object_realize (self);
	return self->pv->id;
}

GQuark
seahorse_object_get_tag (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0);
	if (!self->pv->tag)
		seahorse_object_realize (self);
	return self->pv->tag;	
}

SeahorseSource*
seahorse_object_get_source (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->source;	
}

void
seahorse_object_set_source (SeahorseObject *self, SeahorseSource *value)
{
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	
	if (value == self->pv->source)
		return;

	if (self->pv->source) 
		g_object_remove_weak_pointer (G_OBJECT (self->pv->source), (gpointer*)&self->pv->source);

	self->pv->source = value;
	if (self->pv->source != NULL)
		g_object_add_weak_pointer (G_OBJECT (self->pv->source), (gpointer*)&self->pv->source);

	g_object_notify (G_OBJECT (self), "source");
}

SeahorseContext*
seahorse_object_get_context (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->context;
}

SeahorseObject*
seahorse_object_get_preferred (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->preferred;
}

void
seahorse_object_set_preferred (SeahorseObject *self, SeahorseObject *value)
{
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	
	if (self->pv->preferred == value)
		return;

	if (self->pv->preferred)
		g_object_remove_weak_pointer (G_OBJECT (self->pv->preferred), (gpointer*)&self->pv->preferred);

	self->pv->preferred = value;
	if (self->pv->preferred != NULL)
		g_object_add_weak_pointer (G_OBJECT (self->pv->preferred), (gpointer*)&self->pv->preferred);

	g_object_notify (G_OBJECT (self), "preferred");
}

SeahorseObject*
seahorse_object_get_parent (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return self->pv->parent;
}

void
seahorse_object_set_parent (SeahorseObject *self, SeahorseObject *value)
{
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	g_return_if_fail (self->pv->parent != self);
	g_return_if_fail (value != self);
	
	if (value == self->pv->parent)
		return;
	
	/* Set the new parent/child relationship */
	if (self->pv->parent != NULL)
		unregister_child (self->pv->parent, self);

	if (value != NULL)
		register_child (value, self);
	
	g_assert (self->pv->parent == value);

	g_object_notify (G_OBJECT (self), "parent");
}
 
GList*
seahorse_object_get_children (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	seahorse_object_realize (self);
	return g_list_copy (self->pv->children);
}

SeahorseObject*
seahorse_object_get_nth_child (SeahorseObject *self, guint index)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	return SEAHORSE_OBJECT (g_list_nth_data (self->pv->children, index));
}

const gchar*
seahorse_object_get_label (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	seahorse_object_realize (self);
	return self->pv->label;
}

const gchar*
seahorse_object_get_markup (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	seahorse_object_realize (self);
	return self->pv->markup;
}

const gchar*
seahorse_object_get_nickname (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	seahorse_object_realize (self);
	return self->pv->nickname;
}

const gchar*
seahorse_object_get_description (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	seahorse_object_realize (self);
	return self->pv->description;
}

const gchar*
seahorse_object_get_icon (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	seahorse_object_realize (self);
	return self->pv->icon;
}

const gchar*
seahorse_object_get_identifier (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	seahorse_object_realize (self);
	return self->pv->identifier;	
}

SeahorseLocation
seahorse_object_get_location (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), SEAHORSE_LOCATION_INVALID);
	if (self->pv->location == SEAHORSE_LOCATION_INVALID)
		seahorse_object_realize (self);
	return self->pv->location;
}

SeahorseUsage
seahorse_object_get_usage (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), SEAHORSE_USAGE_NONE);
	if (self->pv->usage == SEAHORSE_USAGE_NONE)
		seahorse_object_realize (self);
	return self->pv->usage;	
}

guint
seahorse_object_get_flags (SeahorseObject *self)
{
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), 0);
	seahorse_object_realize (self);
	return self->pv->flags;	
}

gboolean
seahorse_object_lookup_property (SeahorseObject *self, const gchar *field, GValue *value)
{
	GParamSpec *spec;
	
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), FALSE);
	g_return_val_if_fail (field, FALSE);
	g_return_val_if_fail (value, FALSE);
	
	spec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), field);
	if (!spec) {
		/* Some name mapping to new style names */
		if (g_str_equal (field, "simple-name"))
			field = "nickname";
		else if (g_str_equal (field, "key-id"))
			field = "identifier";
		else if (g_str_equal (field, "display-name"))
			field = "label";
		else if (g_str_equal (field, "key-desc"))
			field = "description";
		else if (g_str_equal (field, "ktype"))
			field = "tag";
		else if (g_str_equal (field, "etype"))
			field = "usage";
		else if (g_str_equal (field, "display-id"))
			field = "identifier";
		else if (g_str_equal (field, "stock-id"))
			field = "icon";
		else 
			return FALSE;
	
		/* Try again */
		spec = g_object_class_find_property (G_OBJECT_GET_CLASS (self), field);
		if (!spec)
			return FALSE;
	}

	g_value_init (value, spec->value_type);
	g_object_get_property (G_OBJECT (self), field, value);
	return TRUE; 
}

void
seahorse_object_realize (SeahorseObject *self)
{
	SeahorseObjectClass *klass;
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	if (self->pv->realized)
		return;
	if (self->pv->realizing)
		return;
	klass = SEAHORSE_OBJECT_GET_CLASS (self);
	g_return_if_fail (klass->realize);
	self->pv->realizing = TRUE;
	(klass->realize) (self);
	self->pv->realizing = FALSE;
}

void
seahorse_object_refresh (SeahorseObject *self)
{
	SeahorseObjectClass *klass;
	g_return_if_fail (SEAHORSE_IS_OBJECT (self));
	klass = SEAHORSE_OBJECT_GET_CLASS (self);
	g_return_if_fail (klass->refresh);
	(klass->refresh) (self);
}

SeahorseOperation*
seahorse_object_delete (SeahorseObject *self)
{
	SeahorseObjectClass *klass;
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (self), NULL);
	klass = SEAHORSE_OBJECT_GET_CLASS (self);
	g_return_val_if_fail (klass->delete, NULL);
	return (klass->delete) (self);
}

gboolean 
seahorse_object_predicate_match (SeahorseObjectPredicate *self, SeahorseObject* obj) 
{
	SeahorseObjectPrivate *pv;
	
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (obj), FALSE);
	pv = obj->pv;
	
	seahorse_object_realize (obj);
	
	/* Check all the fields */
	if (self->tag != 0 && self->tag != pv->tag)
		return FALSE;
	if (self->id != 0 && self->id != pv->id)
		return FALSE;
	if (self->type != 0 && self->type != G_OBJECT_TYPE (obj))
		return FALSE;
	if (self->location != 0 && self->location != pv->location)
		return FALSE;
	if (self->usage != 0 && self->usage != pv->usage) 
		return FALSE;
	if (self->flags != 0 && (self->flags & pv->flags) == 0) 
		return FALSE;
	if (self->nflags != 0 && (self->nflags & pv->flags) != 0)
		return FALSE;
	if (self->source != NULL && self->source != pv->source)
		return FALSE;

	/* And any custom stuff */
	if (self->custom != NULL && !self->custom (obj, self->custom_target)) 
		return FALSE;

	return TRUE;
}

void
seahorse_object_predicate_clear (SeahorseObjectPredicate *self)
{
	memset (self, 0, sizeof (*self));
}
