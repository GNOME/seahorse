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

#include "seahorse-gkr-item-commands.h"

#include "seahorse-gkr.h"
#include "seahorse-gkr-item.h"
#include "seahorse-gkr-dialogs.h"

#include "common/seahorse-registry.h"

#include "seahorse-source.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (SeahorseGkrItemCommands, seahorse_gkr_item_commands, SEAHORSE_TYPE_COMMANDS);

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gkr_item_commands_show_properties (SeahorseCommands* base, SeahorseObject* object) 
{
	GtkWindow *window;

	g_return_if_fail (SEAHORSE_IS_OBJECT (object));
	g_return_if_fail (seahorse_object_get_tag (object) == SEAHORSE_GKR_TYPE);

	window = seahorse_view_get_window (seahorse_commands_get_view (base));
	if (G_OBJECT_TYPE (object) == SEAHORSE_TYPE_GKR_ITEM) 
		seahorse_gkr_item_properties_show (SEAHORSE_GKR_ITEM (object), window); 
	
	else
		g_return_if_reached ();
}

static SeahorseOperation* 
seahorse_gkr_item_commands_delete_objects (SeahorseCommands* base, GList* objects) 
{
	gchar *prompt;
	GtkWidget *parent;
	gboolean ret;
	guint num;

	num = g_list_length (objects);
	if (num == 0)
		return NULL;

	if (num == 1) {
		prompt = g_strdup_printf (_ ("Are you sure you want to delete the password '%s'?"), 
		                          seahorse_object_get_label (SEAHORSE_OBJECT (objects->data)));
	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to delete %d password?", 
		                                    "Are you sure you want to delete %d passwords?", 
		                                    num), num);
	}
	
	parent = GTK_WIDGET (seahorse_view_get_window (seahorse_commands_get_view (base)));
	ret = seahorse_util_prompt_delete (prompt, parent);
	g_free (prompt);
	
	if (!ret)
		return NULL;

	return seahorse_source_delete_objects (objects);
}

static GObject* 
seahorse_gkr_item_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_gkr_item_commands_parent_class)->constructor (type, n_props, props);
	SeahorseCommands *base = NULL;
	SeahorseView *view;
	
	if (obj) {
		base = SEAHORSE_COMMANDS (obj);
		view = seahorse_commands_get_view (base);
		g_return_val_if_fail (view, NULL);
		seahorse_view_register_commands (view, base, SEAHORSE_TYPE_GKR_ITEM);
	}
	
	return obj;
}

static void
seahorse_gkr_item_commands_init (SeahorseGkrItemCommands *self)
{

}

static void
seahorse_gkr_item_commands_class_init (SeahorseGkrItemCommandsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseCommandsClass *cmd_class = SEAHORSE_COMMANDS_CLASS (klass);
	
	seahorse_gkr_item_commands_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructor = seahorse_gkr_item_commands_constructor;

	cmd_class->show_properties = seahorse_gkr_item_commands_show_properties;
	cmd_class->delete_objects = seahorse_gkr_item_commands_delete_objects;

	/* Register this class as a commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_TYPE_GKR_ITEM_COMMANDS, 
	                                 SEAHORSE_GKR_TYPE_STR, "commands", NULL, NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

