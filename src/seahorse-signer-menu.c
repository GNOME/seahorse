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

#include "seahorse-signer-menu.h"
#include "seahorse-signer-menu-item.h"

enum {
	PROP_0,
	PROP_CTX
};

static void	seahorse_signer_menu_class_init		(SeahorseSignerMenuClass	*klass);
static void	seahorse_signer_menu_finalize		(GObject			*gobject);
static void	seahorse_signer_menu_set_property	(GObject			*gobject,
							 guint				prop_id,
							 const GValue			*value,
							 GParamSpec			*pspec);
static void	seahorse_signer_menu_get_property	(GObject			*gobject,
							 guint				prop_id,
							 GValue				*value,
							 GParamSpec			*pspec);

static void	seahorse_signer_menu_remove		(GtkContainer			*container,
							 GtkWidget			*widget);
/* Context signals */
static void	seahorse_signer_menu_context_destroyed	(GtkObject			*object,
							 SeahorseSignerMenu		*smenu);
static void	seahorse_signer_menu_key_added		(SeahorseContext		*sctx,
							 SeahorseKey			*skey,
							 SeahorseSignerMenu		*smenu);
/* Item signal */
static void	seahorse_signer_menu_item_activate	(GtkMenuItem			*item,
							 SeahorseSignerMenu		*smenu);

static GtkMenuClass	*parent_class	= NULL;

GType
seahorse_signer_menu_get_type (void)
{
	static GType signer_menu_type = 0;
	
	if (!signer_menu_type) {
		static const GTypeInfo signer_menu_info =
		{
			sizeof (SeahorseSignerMenuClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_signer_menu_class_init,
			NULL, NULL,
			sizeof (SeahorseSignerMenu),
			0, NULL
		};
		
		signer_menu_type = g_type_register_static (GTK_TYPE_MENU,
			"SeahorseSignerMenu", &signer_menu_info, 0);
	}
	
	return signer_menu_type;
}

static void
seahorse_signer_menu_class_init (SeahorseSignerMenuClass *klass)
{
	GObjectClass *gobject_class;
	GtkContainerClass *container_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	container_class = GTK_CONTAINER_CLASS (klass);
	
	gobject_class->finalize = seahorse_signer_menu_finalize;
	gobject_class->set_property = seahorse_signer_menu_set_property;
	gobject_class->get_property = seahorse_signer_menu_get_property;
	
	container_class->remove = seahorse_signer_menu_remove;
	
	g_object_class_install_property (gobject_class, PROP_CTX,
		g_param_spec_object ("ctx", "Seahorse Context",
				     "Current Seahorse Context to use",
				     SEAHORSE_TYPE_CONTEXT, G_PARAM_READWRITE));
}

static void
seahorse_signer_menu_finalize (GObject *gobject)
{
	SeahorseSignerMenu *smenu;
	
	smenu = SEAHORSE_SIGNER_MENU (gobject);
	
	g_signal_handlers_disconnect_by_func (GTK_OBJECT (smenu->sctx),
		seahorse_signer_menu_context_destroyed, smenu);
	g_signal_handlers_disconnect_by_func (smenu->sctx,
		seahorse_signer_menu_key_added, smenu);
	g_object_unref (smenu->sctx);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_signer_menu_set_property (GObject *gobject, guint prop_id,
				   const GValue *value, GParamSpec *pspec)
{
	SeahorseSignerMenu *smenu;
	
	smenu = SEAHORSE_SIGNER_MENU (gobject);
	
	switch (prop_id) {
		case PROP_CTX:
			smenu->sctx = g_value_get_object (value);
			g_object_ref (smenu->sctx);
			g_signal_connect_after (GTK_OBJECT (smenu->sctx), "destroy",
				G_CALLBACK (seahorse_signer_menu_context_destroyed), smenu);
			g_signal_connect_after (smenu->sctx, "add",
				G_CALLBACK (seahorse_signer_menu_key_added), smenu);
			break;
		default:
			break;
	}
}

static void
seahorse_signer_menu_get_property (GObject *gobject, guint prop_id,
				   GValue *value, GParamSpec *pspec)
{
	SeahorseSignerMenu *smenu;
	
	smenu = SEAHORSE_SIGNER_MENU (gobject);
	
	switch (prop_id) {
		case PROP_CTX:
			g_value_set_object (value, smenu->sctx);
			break;
		default:
			break;
	}
}

static void
seahorse_signer_menu_remove (GtkContainer *container, GtkWidget *widget)
{
	g_signal_handlers_disconnect_by_func (GTK_MENU_ITEM (widget),
		seahorse_signer_menu_item_activate, SEAHORSE_SIGNER_MENU (container));
	
	GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
seahorse_signer_menu_context_destroyed (GtkObject *object, SeahorseSignerMenu *smenu)
{
	gtk_widget_destroy (GTK_WIDGET (smenu));
}

static gboolean
append_key (SeahorseSignerMenu *smenu, SeahorseKey *skey)
{
	GtkWidget *widget;
	
	if (seahorse_key_can_sign (skey) && 
	seahorse_context_key_has_secret (smenu->sctx, skey)) {
		widget = seahorse_signer_menu_item_new (skey);
		gtk_menu_shell_append (GTK_MENU_SHELL (smenu), widget);
		g_signal_connect_after (GTK_MENU_ITEM (widget), "activate",
			G_CALLBACK (seahorse_signer_menu_item_activate), smenu);
		return TRUE;
	}
	else
		return FALSE;
}

static void
seahorse_signer_menu_key_added (SeahorseContext *sctx, SeahorseKey *skey,
				SeahorseSignerMenu *smenu)
{
	append_key (smenu, skey);
}

static void
seahorse_signer_menu_item_activate (GtkMenuItem *item, SeahorseSignerMenu *smenu)
{
	seahorse_context_set_signer (smenu->sctx, SEAHORSE_SIGNER_MENU_ITEM (item)->skey);
}

void
seahorse_signer_menu_new (SeahorseContext *sctx, GtkOptionMenu *optionmenu)
{
	GtkWidget *widget;
	GList *list = NULL;
	SeahorseKey *skey, *signer = NULL;
	gint index = 0, history = 0;
	
	widget = g_object_new (SEAHORSE_TYPE_SIGNER_MENU, "ctx", sctx, NULL);
	
	list = seahorse_context_get_keys (sctx);
	signer = seahorse_context_get_last_signer (sctx);
	
	while (list != NULL && (skey = list->data) != NULL) {
		if (append_key (SEAHORSE_SIGNER_MENU (widget), skey)) {
			if (signer == NULL && index == 0)
				gtk_menu_item_activate (GTK_MENU_ITEM (widget));
			else if (signer != NULL && g_str_equal (
			seahorse_key_get_keyid (skey, 0),
			seahorse_key_get_keyid (signer, 0)))
				history = index;
			index++;
		}
		list = g_list_next (list);
	}
	
	gtk_option_menu_set_menu (optionmenu, widget);
	gtk_option_menu_set_history (optionmenu, history);
	
	gtk_widget_show_all (GTK_WIDGET (optionmenu));
}
