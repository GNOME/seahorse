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

#define PROPERTIES "key-properties"

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

/* Hash set of widget hash sets with the key's id as the key */
static GHashTable		*types			= NULL;
/* Hash set of window groups with the key's id as the key */
static GHashTable		*groups			= NULL;

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
	GHashTable *widgets = NULL;
	GtkWindowGroup *group = NULL;
	const gchar *id;
	GtkWidget *widget;
	
	skwidget = SEAHORSE_KEY_WIDGET (gobject);
	swidget = SEAHORSE_WIDGET (skwidget);
	
	id = seahorse_key_get_keyid (skwidget->skey);
	
	/* get widgets hash from types */
	widgets = g_hash_table_lookup (types, id);
	/* if have a widgets hash, remove the widget */
	if (widgets != NULL) {
		g_hash_table_remove (widgets, swidget->name);
		/* if there are no more widgets, remove the hash */
		if (g_hash_table_size (widgets) == 0) {
			g_hash_table_remove (types, id);
			/* if there are no more keys, destroy types */
			if (g_hash_table_size (types) == 0) {
				g_hash_table_destroy (types);
				types = NULL;
			}
		}
	}
	
	/* get group from groups */
	group = g_hash_table_lookup (groups, id);
	/* if have a group, remove grab & window */
	if (group != NULL) {
		widget = glade_xml_get_widget (swidget->xml, swidget->name);
		gtk_grab_remove (widget);
		gtk_window_group_remove_window (group, GTK_WINDOW (widget));
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

/* Tries to create a new key widget given parameters.
 * Makes use of hash sets to control which widgets can be created and how
 * the focus is grabbed.
 */
static SeahorseWidget*
seahorse_key_widget_create (gchar *name, SeahorseKey *skey, guint index)
{
	SeahorseWidget *swidget = NULL;	//widget to lookup or create
	GHashTable *widgets = NULL;	//hash of widgets from types
	const gchar *id;		//hash key
	GtkWindowGroup *group = NULL;	//window group from groups
	GtkWidget *widget;		//main window widget of swidget
	
	id = seahorse_key_get_keyid (skey);
	
	/* if don't have a types hash, create one */
	if (types == NULL)
		types = g_hash_table_new_full ((GHashFunc)g_str_hash, (GCompareFunc)g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify)g_hash_table_destroy);
	/* otherwise lookup the widgets hash for the key */
	else
		widgets = g_hash_table_lookup (types, id);
	
	/* if don't have a widgets hash for a key, create one and insert it */
	if (widgets == NULL) {
		widgets = g_hash_table_new_full ((GHashFunc)g_str_hash,
			(GCompareFunc)g_str_equal, (GDestroyNotify)g_free, NULL);
		g_hash_table_insert (types, g_strdup (id), widgets);
	}
	/* otherwise lookup the widget */
	else
		swidget = g_hash_table_lookup (widgets, name);
	
	/* if already have a widget of that type for the key, return null */
	if (swidget != NULL)
		return NULL;
	
	/* otherwise create a new widget & insert into widgets hash for the key */
	swidget = g_object_new (SEAHORSE_TYPE_KEY_WIDGET, "name", name,
		                    "key", skey, "index", index, NULL);
	g_hash_table_insert (widgets, g_strdup (name), swidget);
	
	/* if don't have a groups hash, create one */
	if (groups == NULL)
		groups = g_hash_table_new_full ((GHashFunc)g_str_hash, (GCompareFunc)g_str_equal,
			(GDestroyNotify)g_free, (GDestroyNotify)g_object_unref);
	/* otherwise lookup the group for that key */
	else
		group = g_hash_table_lookup (groups, id);
	
	/* if don't have a group for the key, create one and insert it */
	if (group == NULL) {
		group = gtk_window_group_new ();
		g_hash_table_insert (groups, g_strdup (id), group);
	}
	
	/* get window, add it to the group, grab it, return it */
	widget = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_window_group_add_window (group, GTK_WINDOW (widget));
	gtk_grab_add (widget);
	
	return swidget;
}

SeahorseWidget*
seahorse_key_widget_new (gchar *name, SeahorseKey *skey)
{
	return seahorse_key_widget_create (name, skey, 0);
}

SeahorseWidget*
seahorse_key_widget_new_with_index (gchar *name, SeahorseKey *skey, guint index)
{
	return seahorse_key_widget_create (name, skey, index);
}

gboolean
seahorse_key_widget_can_create (gchar *name, SeahorseKey *skey)
{
	GHashTable *widgets = NULL;
	
	if (types == NULL)
		return TRUE;
	
	widgets = g_hash_table_lookup (types, seahorse_key_get_keyid (skey));
	
	if (widgets == NULL)
		return TRUE;
	
	return (g_hash_table_lookup (widgets, name) == NULL);
}
