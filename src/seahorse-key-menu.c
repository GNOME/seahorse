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

#include "seahorse-key-menu.h"

enum {
	PROP_0,
	PROP_SECRET,
	PROP_CTX
};

enum {
	SELECTED,
	LAST_SIGNAL
};

static void	seahorse_key_menu_class_init		(SeahorseKeyMenuClass	*klass);
static void	seahorse_key_menu_finalize		(GObject		*gobject);
static void	seahorse_key_menu_set_property		(GObject		*object,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void	seahorse_key_menu_get_property		(GObject		*object,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);
/* Signal functions */
static void	seahorse_key_menu_add_key		(SeahorseContext	*sctx,
							 SeahorseKey		*skey,
							 SeahorseKeyMenu	*skmenu);
static void	seahorse_key_menu_item_activated	(GtkMenuItem		*menuitem,
							 SeahorseKeyMenu	*skmenu);
static void	seahorse_key_menu_item_removed		(GtkContainer		*container,
							 GtkWidget		*widget);

static GtkMenuClass	*parent_class	= NULL;
static guint		menu_signals[LAST_SIGNAL]	= { 0 };

GType
seahorse_key_menu_get_type (void)
{
	static GType key_menu_type = 0;
	
	if (!key_menu_type) {
		static const GTypeInfo key_menu_info =
		{
			sizeof (SeahorseKeyMenuClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_key_menu_class_init,
			NULL, NULL,
			sizeof (SeahorseKeyMenu),
			0, NULL
		};
		
		key_menu_type = g_type_register_static (GTK_TYPE_MENU,
			"SeahorseKeyMenu", &key_menu_info, 0);
	}
	
	return key_menu_type;
}

static void
seahorse_key_menu_class_init (SeahorseKeyMenuClass *klass)
{
	GObjectClass *gobject_class;
	GtkContainerClass *container_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	container_class = GTK_CONTAINER_CLASS (klass);
	
	gobject_class->finalize = seahorse_key_menu_finalize;
	gobject_class->set_property = seahorse_key_menu_set_property;
	gobject_class->get_property = seahorse_key_menu_get_property;
	
	container_class->remove = seahorse_key_menu_item_removed;
	
	g_object_class_install_property (gobject_class, PROP_SECRET,
		g_param_spec_boolean ("secret", _("Menu Keys have Secret"),
				      _("Whether the menu contains keys that have secret components"),
				      FALSE, G_PARAM_READWRITE));
	
	g_object_class_install_property (gobject_class, PROP_CTX,
		g_param_spec_object ("ctx", _("Seahorse Context"),
				     _("Current Seahorse Context to use"),
				     SEAHORSE_TYPE_CONTEXT, G_PARAM_READWRITE));
	
	menu_signals[SELECTED] = g_signal_new ("selected", G_OBJECT_CLASS_TYPE (gobject_class),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (SeahorseKeyMenuClass, selected),
		NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, SEAHORSE_TYPE_KEY);
}

static void
seahorse_key_menu_finalize (GObject *gobject)
{
	SeahorseKeyMenu *skmenu;
	
	skmenu = SEAHORSE_KEY_MENU (gobject);
	
	g_signal_handlers_disconnect_by_func (skmenu->sctx, seahorse_key_menu_add_key, skmenu);
	g_object_unref (skmenu->sctx);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/* Appends a new SeahorseKeyMenuItem to menu */
static void
append_key (SeahorseKeyMenu *skmenu, SeahorseKey *skey)
{
	GtkWidget *item;
	
	if (skmenu->have_secret)
		g_return_if_fail (seahorse_context_key_has_secret (skmenu->sctx, skey));
	
	item = seahorse_key_menu_item_new (skey);
	g_signal_connect (GTK_MENU_ITEM (item), "activate", G_CALLBACK (seahorse_key_menu_item_activated), skmenu);
	
	gtk_menu_shell_append (GTK_MENU_SHELL (skmenu), item);
	gtk_widget_show_all (item);
}

/* Appends key to menu */
static void
seahorse_key_menu_add_key (SeahorseContext *sctx, SeahorseKey *skey, SeahorseKeyMenu *skmenu)
{
	append_key (skmenu, skey);}

/* Emit the 'selected' signal when a key item has been activated */
static void
seahorse_key_menu_item_activated (GtkMenuItem *menuitem, SeahorseKeyMenu *skmenu)
{
	SeahorseKeyMenuItem *skitem;
	
	skitem = SEAHORSE_KEY_MENU_ITEM (menuitem);
	g_signal_emit (G_OBJECT (skmenu), menu_signals[SELECTED], 0, skitem->skey);
}

/* Disconnect the key item's activate signal */
static void
seahorse_key_menu_item_removed (GtkContainer *container, GtkWidget *widget)
{
	g_signal_handlers_disconnect_by_func (GTK_MENU_ITEM (widget), seahorse_key_menu_item_activated, SEAHORSE_KEY_MENU (container));
	
	GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
seahorse_key_menu_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SeahorseKeyMenu *skmenu;
	GList *list = NULL;
	SeahorseKey *skey;
	
	skmenu = SEAHORSE_KEY_MENU (object);
	
	switch (prop_id) {
		case PROP_SECRET:
			skmenu->have_secret = g_value_get_boolean (value);
			break;
		/* Connects signals & loads menu */
		case PROP_CTX:
			skmenu->sctx = g_value_get_object (value);
			g_object_ref (skmenu->sctx);
			g_signal_connect (skmenu->sctx, "add",
				G_CALLBACK (seahorse_key_menu_add_key), skmenu);
		
			list = seahorse_context_get_keys (skmenu->sctx);
			while (list != NULL && (skey = list->data) != NULL) {
				if (!skmenu->have_secret || (skmenu->have_secret && seahorse_context_key_has_secret (skmenu->sctx, skey)))
					append_key (skmenu, skey);
				list = g_list_next (list);
			}
			break;
		default:
			break;
	}
}

static void
seahorse_key_menu_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SeahorseKeyMenu *skmenu;
	
	skmenu = SEAHORSE_KEY_MENU (object);
	
	switch (prop_id) {
		case PROP_SECRET:
			g_value_set_boolean (value, skmenu->have_secret);
			break;
		case PROP_CTX:
			g_value_set_object (value, skmenu->sctx);
			break;
		default:
			break;
	}
}

GtkWidget*
seahorse_key_menu_new (gboolean have_secret, SeahorseContext *sctx)
{
	GtkWidget *widget;
	
	widget = g_object_new (SEAHORSE_TYPE_KEY_MENU, "secret", have_secret, "ctx", sctx, NULL);
	
	gtk_widget_show_all (widget);
	return widget;
}
