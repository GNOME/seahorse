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

#include "seahorse-gkr.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-operation.h"

enum {
	PROP_0,
	PROP_KEYRING_NAME,
	PROP_KEYRING_INFO
};

struct _SeahorseGkrKeyringPrivate {
	gchar *keyring_name;
	
	gpointer req_info;
	GnomeKeyringInfo *keyring_info;
};

G_DEFINE_TYPE (SeahorseGkrKeyring, seahorse_gkr_keyring, SEAHORSE_TYPE_OBJECT);

GType
boxed_type_keyring_info (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("GnomeKeyringInfo", 
		                                     (GBoxedCopyFunc)gnome_keyring_info_copy,
		                                     (GBoxedFreeFunc)gnome_keyring_info_free);
	return type;
}

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void
received_keyring_info (GnomeKeyringResult result, GnomeKeyringInfo *info, gpointer data)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (data);
	self->pv->req_info = NULL;
	
	if (result == GNOME_KEYRING_RESULT_CANCELLED)
		return;
	
	if (result != GNOME_KEYRING_RESULT_OK) {
		/* TODO: Implement so that we can display an error icon, along with some text */
		g_message ("failed to retrieve info for keyring %s: %s",
		           self->pv->keyring_name, 
		           gnome_keyring_result_to_message (result));
		return;
	}

	seahorse_gkr_keyring_set_info (self, info);
}

static void
load_keyring_info (SeahorseGkrKeyring *self)
{
	/* Already in progress */
	if (!self->pv->req_info) {
		g_object_ref (self);
		self->pv->req_info = gnome_keyring_get_info (self->pv->keyring_name,
		                                             received_keyring_info,
		                                             self, g_object_unref);
	}
}

static gboolean
require_keyring_info (SeahorseGkrKeyring *self)
{
	if (!self->pv->keyring_info)
		load_keyring_info (self);
	return self->pv->keyring_info != NULL;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_gkr_keyring_realize (SeahorseObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	gchar *name, *markup;
	
	name = g_strdup_printf ("Passwords: %s", self->pv->keyring_name);
	markup = g_markup_printf_escaped ("<b>Passwords:</b> %s", self->pv->keyring_name);
	
	g_object_set (self,
	              "label", name,
	              "markup", markup,
	              "nickname", self->pv->keyring_name,
	              "identifier", "",
	              "description", "",
	              "flags", 0,
	              "icon", "folder",
	              NULL);
	
	g_free (name);
	g_free (markup);
	
	SEAHORSE_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->realize (obj);
}

static void
seahorse_gkr_keyring_refresh (SeahorseObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);

	if (self->pv->keyring_info)
		load_keyring_info (self);
	
	SEAHORSE_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->refresh (obj);
}

static SeahorseOperation*
seahorse_gkr_keyring_delete (SeahorseObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	return seahorse_gkr_operation_delete_keyring (self);
}


static GObject* 
seahorse_gkr_keyring_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->constructor (type, n_props, props);
	SeahorseGkrKeyring *self = NULL;
	gchar *id;
	
	if (obj) {
		self = SEAHORSE_GKR_KEYRING (obj);
		
		g_return_val_if_fail (self->pv->keyring_name, obj);
		id = g_strdup_printf ("%s:%s", SEAHORSE_GKR_TYPE_STR, self->pv->keyring_name);
		g_object_set (self, 
		              "id", g_quark_from_string (id), 
		              "usage", SEAHORSE_USAGE_NONE, 
		              NULL);
		g_free (id);
	}
	
	return obj;
}

static void
seahorse_gkr_keyring_init (SeahorseGkrKeyring *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_KEYRING, SeahorseGkrKeyringPrivate);
}

static void
seahorse_gkr_keyring_finalize (GObject *obj)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	
	if (self->pv->keyring_info)
		gnome_keyring_info_free (self->pv->keyring_info);
	self->pv->keyring_info = NULL;
	g_assert (self->pv->req_info == NULL);
    
	g_free (self->pv->keyring_name);
	self->pv->keyring_name = NULL;
	
	if (self->pv->keyring_info) 
		gnome_keyring_info_free (self->pv->keyring_info);
	self->pv->keyring_info = NULL;
	
	G_OBJECT_CLASS (seahorse_gkr_keyring_parent_class)->finalize (obj);
}

static void
seahorse_gkr_keyring_set_property (GObject *obj, guint prop_id, const GValue *value, 
                                   GParamSpec *pspec)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	
	switch (prop_id) {
	case PROP_KEYRING_NAME:
		g_return_if_fail (self->pv->keyring_name == NULL);
		self->pv->keyring_name = g_value_dup_string (value);
		g_object_notify (obj, "keyring-name");
		break;
	case PROP_KEYRING_INFO:
		seahorse_gkr_keyring_set_info (self, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_keyring_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	SeahorseGkrKeyring *self = SEAHORSE_GKR_KEYRING (obj);
	
	switch (prop_id) {
	case PROP_KEYRING_NAME:
		g_value_set_string (value, seahorse_gkr_keyring_get_name (self));
		break;
	case PROP_KEYRING_INFO:
		g_value_set_boxed (value, seahorse_gkr_keyring_get_info (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_gkr_keyring_class_init (SeahorseGkrKeyringClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseObjectClass *seahorse_class;
	
	seahorse_gkr_keyring_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGkrKeyringPrivate));

	gobject_class->constructor = seahorse_gkr_keyring_constructor;
	gobject_class->finalize = seahorse_gkr_keyring_finalize;
	gobject_class->set_property = seahorse_gkr_keyring_set_property;
	gobject_class->get_property = seahorse_gkr_keyring_get_property;
    
	seahorse_class = SEAHORSE_OBJECT_CLASS (klass);
	seahorse_class->realize = seahorse_gkr_keyring_realize;
	seahorse_class->refresh = seahorse_gkr_keyring_refresh;
	seahorse_class->delete = seahorse_gkr_keyring_delete;
	
	g_object_class_install_property (gobject_class, PROP_KEYRING_NAME,
	           g_param_spec_string ("keyring-name", "Gnome Keyring Name", "Name of keyring.", 
	                                "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (gobject_class, PROP_KEYRING_INFO,
	           g_param_spec_boxed ("keyring-info", "Gnome Keyring Info", "Info about keyring.", 
	                               boxed_type_keyring_info (), G_PARAM_READWRITE));
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseGkrKeyring*
seahorse_gkr_keyring_new (const gchar *keyring_name)
{
	return g_object_new (SEAHORSE_TYPE_GKR_KEYRING, "keyring-name", keyring_name, NULL);
}

const gchar*
seahorse_gkr_keyring_get_name (SeahorseGkrKeyring *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (self), NULL);
	return self->pv->keyring_name;
}

GnomeKeyringInfo*
seahorse_gkr_keyring_get_info (SeahorseGkrKeyring *self)
{
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (self), NULL);
	require_keyring_info (self);
	return self->pv->keyring_info;
}

void
seahorse_gkr_keyring_set_info (SeahorseGkrKeyring *self, GnomeKeyringInfo *info)
{
	GObject *obj;
	
	g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (self));
	
	if (self->pv->keyring_info)
		gnome_keyring_info_free (self->pv->keyring_info);
	if (info)
		self->pv->keyring_info = gnome_keyring_info_copy (info);
	else
		self->pv->keyring_info = NULL;
	
	obj = G_OBJECT (self);
	g_object_freeze_notify (obj);
	seahorse_gkr_keyring_realize (SEAHORSE_OBJECT (self));
	g_object_notify (obj, "keyring-info");
	g_object_thaw_notify (obj);
}
