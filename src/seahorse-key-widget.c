/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <gnome.h>

#include "seahorse-key-widget.h"

enum {
	PROP_0,
	PROP_KEY,
	PROP_INDEX
};

static void	seahorse_key_widget_class_init		(SeahorseKeyWidgetClass	*klass);
static void	seahorse_key_widget_finalize		(GObject		*gobject);
static void	seahorse_key_widget_set_property	(GObject		*object,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void	seahorse_key_widget_get_property	(GObject		*object,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);
/* signal functions */
static void	seahorse_key_widget_destroyed		(GtkObject		*skey,
							 SeahorseKeyWidget	*skwidget);

static SeahorseWidgetClass	*parent_class		= NULL;

/* Hash of key widget hashes with name as key.
 * Key widget hashes have keyid as key and widget as value. */
static GHashTable		*types			= NULL;

GType
seahorse_key_widget_get_type (void)
{
	static GType key_widget_type = 0;
	
	if (!key_widget_type) {
		static const GTypeInfo key_widget_info =
		{
			sizeof (SeahorseKeyWidgetClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_widget_class_init,
			NULL, NULL,
			sizeof (SeahorseKeyWidget),
			0, NULL
		};
		
		key_widget_type = g_type_register_static (SEAHORSE_TYPE_WIDGET,
			"SeahorseKeyWidget",  &key_widget_info, 0);
	}
	
	return key_widget_type;
}

static void
seahorse_key_widget_class_init (SeahorseKeyWidgetClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_key_widget_finalize;
	gobject_class->set_property = seahorse_key_widget_set_property;
	gobject_class->get_property = seahorse_key_widget_get_property;
	
	g_object_class_install_property (gobject_class, PROP_KEY,
		g_param_spec_object ("key",  "Seahorse Key",
				     "Seahorse Key of this widget",
				    SEAHORSE_TYPE_KEY, G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, PROP_INDEX,
		g_param_spec_uint ("index", "Attribute index",
				   "Index of attribute in key, 0 being none",
				   0, G_MAXUINT, 0, G_PARAM_READWRITE));
}

static void
seahorse_key_widget_finalize (GObject *gobject)
{
	SeahorseWidget *swidget;
	SeahorseKeyWidget *skwidget;
	GHashTable *widgets;
	
	skwidget = SEAHORSE_KEY_WIDGET (gobject);
	swidget = SEAHORSE_WIDGET (skwidget);
	
	/* Remove widget from hash */
	widgets = g_hash_table_lookup (types, swidget->name);
	g_hash_table_remove (widgets, seahorse_key_get_keyid (skwidget->skey, 0));
	
	/* Remove key widget hash from types & destroy if empty */
	if (g_hash_table_size (widgets) == 0) {
		g_hash_table_remove (types, swidget->name);
		g_hash_table_destroy (widgets);
		
		/* Destroy types hash if empty */
		if (g_hash_table_size (types) == 0) {
			g_hash_table_destroy (types);
			types = NULL;
		}
	}
	
	g_signal_handlers_disconnect_by_func (GTK_OBJECT (skwidget->skey),
		seahorse_key_widget_destroyed, skwidget);
	
	g_object_unref (skwidget->skey);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_key_widget_set_property (GObject *object, guint prop_id,
				  const GValue *value, GParamSpec *pspec)
{
	SeahorseKeyWidget *skwidget;
	SeahorseWidget *swidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (object);
	swidget = SEAHORSE_WIDGET (object);
	
	switch (prop_id) {
		/* Refs key and connects to 'destroy' signal */
		case PROP_KEY:
			skwidget->skey = g_value_get_object (value);
			g_object_ref (skwidget->skey);
			g_signal_connect_after (GTK_OBJECT (skwidget->skey), "destroy",
				G_CALLBACK (seahorse_key_widget_destroyed), skwidget);
			break;
		case PROP_INDEX:
			skwidget->index = g_value_get_uint (value);
			break;
		default:
			break;
	}
}

static void
seahorse_key_widget_get_property (GObject *object, guint prop_id,
				  GValue *value, GParamSpec *pspec)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (object);
	
	switch (prop_id) {
		case PROP_KEY:
			g_value_set_object (value, skwidget->skey);
			break;
		case PROP_INDEX:
			g_value_set_uint (value, skwidget->index);
		default:
			break;
	}
}

/* Called when skey is destroyed */
static void
seahorse_key_widget_destroyed (GtkObject *skey, SeahorseKeyWidget *skwidget)
{
	seahorse_widget_destroy (SEAHORSE_WIDGET (skwidget));
}

static SeahorseWidget*
seahorse_key_widget_create (gchar *name, SeahorseContext *sctx, SeahorseKey *skey, guint index)
{
	SeahorseWidget *swidget;
	GHashTable *widgets;
	
	/* Check if have types hash */
	if (types != NULL) {
		widgets = g_hash_table_lookup (types, name);
		
		/* Check if have key widgets hash */
		if (widgets != NULL) {
			swidget = g_hash_table_lookup (widgets, seahorse_key_get_keyid (skey, 0));
			
			/* If have widget, present */
			if (swidget != NULL) {
				gtk_window_present (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
				return NULL;
			}
		}
		/* Don't have any key widgets of type */
		else {
			widgets = g_hash_table_new ((GHashFunc)g_str_hash, (GCompareFunc)g_str_equal);
			g_hash_table_insert (types, g_strdup (name), widgets);
		}
	}
	/* Don't have any widgets */
	else {
		types = g_hash_table_new ((GHashFunc)g_str_hash, (GCompareFunc)g_str_equal);
		widgets = g_hash_table_new ((GHashFunc)g_str_hash, (GCompareFunc)g_str_equal);
		g_hash_table_insert (types, g_strdup (name), widgets);
	}
	
	/* If widget doesn't already exist, create & insert into key widgets hash */
	swidget = g_object_new (SEAHORSE_TYPE_KEY_WIDGET, "name", name, "ctx", sctx,
		"key", skey, "index", index, NULL);
	g_hash_table_insert (widgets, (gchar*)seahorse_key_get_keyid (skey, 0), swidget);
	
	return swidget;
}

SeahorseWidget*
seahorse_key_widget_new (gchar *name, SeahorseContext *sctx, SeahorseKey *skey)
{
	return seahorse_key_widget_create (name, sctx, skey, 0);
}

SeahorseWidget*
seahorse_key_widget_new_with_index (gchar *name, SeahorseContext *sctx,
				    SeahorseKey *skey, guint index)
{
	return seahorse_key_widget_create (name, sctx, skey, index);
}
