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
#include <eel/eel.h>

#include "seahorse-signer-menu-item.h"
#include "seahorse-context.h"

enum {
	PROP_0,
	PROP_KEY
};

static void	seahorse_signer_menu_item_class_init	(SeahorseSignerMenuItemClass	*klass);
static void	seahorse_signer_menu_item_finalize	(GObject			*gobject);
static void	seahorse_signer_menu_item_set_property	(GObject			*gobject,
							 guint				prop_id,
							 const GValue			*value,
							 GParamSpec			*pspec);
static void	seahorse_signer_menu_item_get_property	(GObject			*gobject,
							 guint				prop_id,
							 GValue				*value,
							 GParamSpec			*pspec);
static void	seahorse_signer_menu_item_activate	(GtkMenuItem			*item);
/* Key signals */
static void	seahorse_signer_menu_item_destroyed	(GtkObject			*object,
							 SeahorseSignerMenuItem		*sitem);
static void	seahorse_signer_menu_item_changed	(SeahorseKey			*skey,
							 SeahorseKeyChange		change,
							 SeahorseSignerMenuItem		*sitem);

static GtkMenuItemClass	*parent_class	= NULL;

GType
seahorse_signer_menu_item_get_type (void)
{
	static GType signer_menu_item_type = 0;
	
	if (!signer_menu_item_type) {
		static const GTypeInfo signer_menu_item_info =
		{
			sizeof (SeahorseSignerMenuItemClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_signer_menu_item_class_init,
			NULL, NULL,
			sizeof (SeahorseSignerMenuItem),
			0, NULL
		};
		
		signer_menu_item_type = g_type_register_static (GTK_TYPE_MENU_ITEM,
			"SeahorseSignerMenuItem", &signer_menu_item_info, 0);
	}
	
	return signer_menu_item_type;
}

static void
seahorse_signer_menu_item_class_init (SeahorseSignerMenuItemClass *klass)
{
	GObjectClass *gobject_class;
	GtkMenuItemClass *item_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	item_class = GTK_MENU_ITEM_CLASS (klass);
	
	gobject_class->finalize = seahorse_signer_menu_item_finalize;
	gobject_class->set_property = seahorse_signer_menu_item_set_property;
	gobject_class->get_property = seahorse_signer_menu_item_get_property;
	
	item_class->activate = seahorse_signer_menu_item_activate;
	
	g_object_class_install_property (gobject_class, PROP_KEY,
		g_param_spec_object ("key", "Seahorse Key",
				     "Seahorse Key Pair for this item",
				     SEAHORSE_TYPE_KEY_PAIR, G_PARAM_READWRITE));
}

static void
seahorse_signer_menu_item_finalize (GObject *gobject)
{
	SeahorseSignerMenuItem *sitem;
	
	sitem = SEAHORSE_SIGNER_MENU_ITEM (gobject);
	
	g_signal_handlers_disconnect_by_func (GTK_OBJECT (sitem->skpair),
		seahorse_signer_menu_item_destroyed, sitem);
	g_signal_handlers_disconnect_by_func (sitem->skpair,
		seahorse_signer_menu_item_changed, sitem);
	g_object_unref (sitem->skpair);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_signer_menu_item_set_property (GObject *gobject, guint prop_id,
					const GValue *value, GParamSpec *pspec)
{
	SeahorseSignerMenuItem *sitem;
	
	sitem = SEAHORSE_SIGNER_MENU_ITEM (gobject);
	
	switch (prop_id) {
		case PROP_KEY:
			sitem->skpair = g_value_get_object (value);
			g_object_ref (sitem->skpair);
			g_signal_connect_after (GTK_OBJECT (sitem->skpair), "destroy",
				G_CALLBACK (seahorse_signer_menu_item_destroyed), sitem);
			g_signal_connect_after (SEAHORSE_KEY (sitem->skpair), "changed",
				G_CALLBACK (seahorse_signer_menu_item_changed), sitem);
			break;
		default:
			break;
	}
}

static void
seahorse_signer_menu_item_get_property (GObject *gobject, guint prop_id,
					GValue *value, GParamSpec *pspec)
{
	SeahorseSignerMenuItem *sitem;
	
	sitem = SEAHORSE_SIGNER_MENU_ITEM (gobject);
	
	switch (prop_id) {
		case PROP_KEY:
			g_value_set_object (value, sitem->skpair);
			break;
		default:
			break;
	}
}

static void
seahorse_signer_menu_item_activate (GtkMenuItem *item)
{
	SeahorseSignerMenuItem *sitem;
	
	sitem = SEAHORSE_SIGNER_MENU_ITEM (item);
	eel_gconf_set_string (DEFAULT_KEY, seahorse_key_get_id (sitem->skpair->secret));
	
	GTK_MENU_ITEM_CLASS (parent_class)->activate (item);
}

static void
seahorse_signer_menu_item_destroyed (GtkObject *object, SeahorseSignerMenuItem *sitem)
{
	gtk_widget_destroy (GTK_WIDGET (sitem));
}

static void
seahorse_signer_menu_item_changed (SeahorseKey *skey, SeahorseKeyChange change,
				   SeahorseSignerMenuItem *sitem)
{
	switch (change) {
		case SKEY_CHANGE_EXPIRES: case SKEY_CHANGE_DISABLED:
		case SKEY_CHANGE_SUBKEYS:
			if (!seahorse_key_pair_can_sign (SEAHORSE_KEY_PAIR (skey)))
				gtk_widget_destroy (GTK_WIDGET (sitem));
			break;
		case SKEY_CHANGE_UIDS:
			gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (sitem))),
				seahorse_key_get_userid (skey, 0));
			break;
		default:
			break;
	}
}

GtkWidget*
seahorse_signer_menu_item_new (SeahorseKeyPair *skpair)
{
	GtkWidget *widget, *label;
	
	widget = g_object_new (SEAHORSE_TYPE_SIGNER_MENU_ITEM, "key", skpair, NULL);
	
	label = gtk_label_new (seahorse_key_get_userid (SEAHORSE_KEY (skpair), 0));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_container_add (GTK_CONTAINER (widget), label);
	gtk_widget_show_all (widget);
	
	return widget;
}
