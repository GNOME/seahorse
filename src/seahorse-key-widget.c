/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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
	PROP_KEY
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
		g_param_spec_object ("key",  _("Seahorse Key"),
				     _("Seahorse Key of this widget"),
				    SEAHORSE_TYPE_KEY, G_PARAM_READWRITE));
}

static void
seahorse_key_widget_finalize (GObject *gobject)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = SEAHORSE_KEY_WIDGET (gobject);
	
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
			g_signal_connect (GTK_OBJECT (skwidget->skey), "destroy",
				G_CALLBACK (seahorse_key_widget_destroyed), skwidget);
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

SeahorseWidget*
seahorse_key_widget_new (gchar *name, SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = g_object_new (SEAHORSE_TYPE_KEY_WIDGET,
		"name", name, "ctx", sctx, "key", skey,  NULL);
	return SEAHORSE_WIDGET (skwidget);
}

SeahorseWidget*
seahorse_key_widget_new_component (gchar *name, SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseKeyWidget *skwidget;
	
	skwidget = g_object_new (SEAHORSE_TYPE_KEY_WIDGET, "name", name,
		"ctx", sctx, "component", TRUE, "key", skey, NULL);
	return SEAHORSE_WIDGET (skwidget);
}
