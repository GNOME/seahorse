/*
 *  Copyright (C) 2006 Jean-François Rameau and Adam Schreiber
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "seahorse-extension.h"
#include "mozilla/mozilla-helper.h"

#include <epiphany/ephy-extension.h>
#include <epiphany/ephy-embed.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <gmodule.h>
#include <gtk/gtkaction.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkmenuitem.h>

#include <glib/gi18n-lib.h>

#include <string.h>

#define SEAHORSE_EXTENSION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), TYPE_SEAHORSE_EXTENSION, SeahorseExtensionPrivate))

#define LOOKUP_ACTION		"SeahorseExtLookup"
#define WINDOW_DATA_KEY		"SeahorseWindowData"

struct SeahorseExtensionPrivate
{
	int dummy;
};

typedef struct
{
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	guint ui_id;
} WindowData;

static void seahorse_extension_class_init (SeahorseExtensionClass *klass);
static void seahorse_extension_iface_init (EphyExtensionIface *iface);
static void seahorse_extension_init       (SeahorseExtension *extension);

static GObjectClass *parent_class = NULL;

static GType type = 0;

GType
seahorse_extension_get_type (void)
{
	return type;
}

GType
seahorse_extension_register_type (GTypeModule *module)
{
	static const GTypeInfo our_info =
	{
		sizeof (SeahorseExtensionClass),
		NULL, /* base_init */
		NULL, /* base_finalize */
		(GClassInitFunc) seahorse_extension_class_init,
		NULL,
		NULL, /* class_data */
		sizeof (SeahorseExtension),
		0, /* n_preallocs */
		(GInstanceInitFunc) seahorse_extension_init
	};

	static const GInterfaceInfo extension_info =
	{
		(GInterfaceInitFunc) seahorse_extension_iface_init,
		NULL,
		NULL
	};

	type = g_type_module_register_type (module,
					    G_TYPE_OBJECT,
					    "SeahorseExtension",
					    &our_info, 0);

	g_type_module_add_interface (module,
				     type,
				     EPHY_TYPE_EXTENSION,
				     &extension_info);

	return type;
}

static void 
search_seahorse_cb (GtkAction *action,
		    EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	/* ask gecko to encrypt the input if possible */
	mozilla_encrypt (embed);
}

static gboolean
context_menu_cb (EphyEmbed *embed,
		 EphyEmbedEvent *event,
		 EphyWindow *window)
{
	gboolean is_input;
	GtkAction  *action;
	WindowData *data;

	data = (WindowData *) g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_val_if_fail (data != NULL, FALSE);

	action = gtk_action_group_get_action (data->action_group, LOOKUP_ACTION);
	g_return_val_if_fail (action != NULL, FALSE);

	is_input = mozilla_is_input (embed);

	gtk_action_set_sensitive (action, is_input);
	gtk_action_set_visible (action, is_input);

	return FALSE;
}

static void
build_ui (WindowData *data)
{
	GtkUIManager *manager = data->manager;
	guint ui_id;

	LOG ("Building UI");

	/* clean UI */
	if (data->ui_id != 0)
	{
		gtk_ui_manager_remove_ui (manager, data->ui_id);
		gtk_ui_manager_ensure_update (manager);
	}

	data->ui_id = ui_id = gtk_ui_manager_new_merge_id (manager);

	/* Add bookmarks to popup context (normal document) */
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyDocumentPopup",
			       "SeahorseExtSep0", NULL,
			       GTK_UI_MANAGER_SEPARATOR, FALSE);
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyDocumentPopup",
			       LOOKUP_ACTION, LOOKUP_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);

	/* Add bookmarks to input popup context */
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyInputPopup",
			       "SeahorseExtSep0", NULL,
			       GTK_UI_MANAGER_SEPARATOR, FALSE);
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyInputPopup",
			       LOOKUP_ACTION, LOOKUP_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);

	gtk_ui_manager_ensure_update (manager);
}

static void
impl_attach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyTab *tab)
{
	EphyEmbed *embed;

	embed = ephy_tab_get_embed (tab);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	g_signal_connect (embed, "ge_context_menu",
			  G_CALLBACK (context_menu_cb), window);
}

static void
impl_detach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyTab *tab)
{
	EphyEmbed *embed;

	embed = ephy_tab_get_embed (tab);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	g_signal_handlers_disconnect_by_func
		(embed, G_CALLBACK (context_menu_cb), window);
}

static const GtkActionEntry action_entries [] =
{
	{ LOOKUP_ACTION,
	  NULL,
	  N_("_Encrypt"),
	  NULL,
	  NULL,
	  G_CALLBACK (search_seahorse_cb)
	},
};

static void
impl_attach_window (EphyExtension *ext,
		    EphyWindow *window)
{
	GtkActionGroup *action_group;
	WindowData *data;

	LOG ("SeahorseExtension attach_window %p", window);

	/* Attach ui infos to the window */
	data = g_new0 (WindowData, 1);
	g_object_set_data_full (G_OBJECT (window), WINDOW_DATA_KEY, data,
				(GDestroyNotify) g_free);

	/* Create new action group for this extension */
	action_group = gtk_action_group_new ("SeahorseExtActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, action_entries,
				      G_N_ELEMENTS (action_entries), window);

	data->manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	data->action_group = action_group;

	gtk_ui_manager_insert_action_group (data->manager, action_group, -1);
	g_object_unref (action_group);

	/* now add the UI to the window */
	build_ui (data);
}

static void
impl_detach_window (EphyExtension *ext,
		    EphyWindow *window)
{
	WindowData *data;

	LOG ("SeahorseExtension detach_window");

	data = g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_if_fail (data != NULL);

	gtk_ui_manager_remove_ui (data->manager, data->ui_id);
	gtk_ui_manager_ensure_update (data->manager);
	gtk_ui_manager_remove_action_group (data->manager, data->action_group);

	g_object_set_data (G_OBJECT (window), WINDOW_DATA_KEY, NULL);
}

static void
seahorse_extension_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
	iface->attach_tab = impl_attach_tab;
	iface->detach_tab = impl_detach_tab;
}

static void
seahorse_extension_init (SeahorseExtension *extension)
{
	LOG ("SeahorseExtension initialising");

	extension->priv = SEAHORSE_EXTENSION_GET_PRIVATE (extension);
}

static void
seahorse_extension_class_init (SeahorseExtensionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (object_class, sizeof (SeahorseExtensionPrivate));
}
