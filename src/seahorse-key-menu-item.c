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

#include "seahorse-key-menu-item.h"

enum {
	PROP_0,
	PROP_KEY
};

static void	seahorse_key_menu_item_class_init	(SeahorseKeyMenuItemClass	*klass);
static void	seahorse_key_menu_item_finalize		(GObject			*gobject);
static void	seahorse_key_menu_item_set_property	(GObject			*object,
							 guint				prop_id,
							 const GValue			*value,
							 GParamSpec			*pspec);
static void	seahorse_key_menu_item_get_property	(GObject			*object,
							 guint				prop_id,
							 GValue				*value,
							 GParamSpec			*pspec);
/* Signal functions */
static void	seahorse_key_menu_item_key_destroyed	(GtkObject			*object,
							 SeahorseKeyMenuItem		*skitem);

static GtkMenuItemClass	*parent_class	= NULL;

GType
seahorse_key_menu_item_get_type (void)
{
	static GType key_menu_item_type = 0;
	
	if (!key_menu_item_type) {
		static const GTypeInfo key_menu_item_info =
		{
			sizeof (SeahorseKeyMenuItemClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_menu_item_class_init,
			NULL, NULL,
			sizeof (SeahorseKeyMenuItem),
			0, NULL
		};
		
		key_menu_item_type = g_type_register_static (GTK_TYPE_MENU_ITEM,
			"SeahorseKeyMenuItem", &key_menu_item_info, 0);
	}
	
	return key_menu_item_type;
}

static void
seahorse_key_menu_item_class_init (SeahorseKeyMenuItemClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_key_menu_item_finalize;
	gobject_class->set_property = seahorse_key_menu_item_set_property;
	gobject_class->get_property = seahorse_key_menu_item_get_property;
	
	g_object_class_install_property (gobject_class, PROP_KEY,
		g_param_spec_object ("key", _("Seahorse Key"),
				     _("Seahorse Key for this item"),
				     SEAHORSE_TYPE_KEY, G_PARAM_READWRITE));
}

static void
seahorse_key_menu_item_finalize (GObject *gobject)
{
	SeahorseKeyMenuItem *skitem;
	
	skitem = SEAHORSE_KEY_MENU_ITEM (gobject);
	
	g_signal_handlers_disconnect_by_func (GTK_OBJECT (skitem->skey),
		seahorse_key_menu_item_key_destroyed, skitem);
	
	g_object_unref (skitem->skey);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_key_menu_item_set_property (GObject *object, guint prop_id,
				     const GValue *value, GParamSpec *pspec)
{
	SeahorseKeyMenuItem *skitem;
	GtkWidget *accel_label;
	GtkWidget *item;
	
	skitem = SEAHORSE_KEY_MENU_ITEM (object);
	item = GTK_WIDGET (object);
	
	switch (prop_id) {
		/* Displays key's name */
		case PROP_KEY:
			skitem->skey = g_value_get_object (value);
			g_object_ref (skitem->skey);
		
			g_signal_connect (GTK_OBJECT (skitem->skey), "destroy",
				G_CALLBACK (seahorse_key_menu_item_key_destroyed), skitem);
		
			accel_label = gtk_accel_label_new (seahorse_key_get_userid (skitem->skey, 0));
			gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);
			gtk_container_add (GTK_CONTAINER (item), accel_label);
			gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), item);
			gtk_widget_show (accel_label);
			break;
		default:
			break;
	}
}

static void
seahorse_key_menu_item_get_property (GObject *object, guint prop_id,
				     GValue *value, GParamSpec *pspec)
{
	SeahorseKeyMenuItem *skitem;
	
	skitem = SEAHORSE_KEY_MENU_ITEM (object);
	
	switch (prop_id) {
		case PROP_KEY:
			g_value_set_object (value, skitem->skey);
			break;
		default:
			break;
	}
}

/* Removes itself from the menu if the SeahorseKey is destroyed. */
static void
seahorse_key_menu_item_key_destroyed (GtkObject *object, SeahorseKeyMenuItem *skitem)
{
	GtkWidget *widget;
	
	widget = GTK_WIDGET (skitem);
	
	/* Remove & unref key */
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	g_object_unref (object);
}

GtkWidget*
seahorse_key_menu_item_new (SeahorseKey *skey)
{
	return g_object_new (SEAHORSE_TYPE_KEY_MENU_ITEM, "key", skey, NULL);
}
