/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
#include "seahorse-gkr-operation.h"

#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-source.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (SeahorseGkrItemCommands, seahorse_gkr_item_commands, SEAHORSE_TYPE_COMMANDS);

static SeahorsePredicate commands_predicate = { 0, };

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gkr_item_commands_show_properties (SeahorseCommands* base,
                                            GObject* object)
{
	GtkWindow *window;

	window = seahorse_commands_get_window (base);
	if (G_OBJECT_TYPE (object) == SEAHORSE_TYPE_GKR_ITEM) 
		seahorse_gkr_item_properties_show (SEAHORSE_GKR_ITEM (object), window); 
	
	else
		g_return_if_reached ();
}

static void
on_delete_objects (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
	SeahorseCommands *commands = SEAHORSE_COMMANDS (user_data);
	GError *error = NULL;
	GtkWidget *parent;

	if (!seahorse_gkr_delete_finish (result, &error)) {
		parent = GTK_WIDGET (seahorse_commands_get_window (commands));
		seahorse_util_show_error (parent, _("Couldn't delete item"), error->message);
		g_error_free (error);
	}

	g_object_unref (commands);
}

static gboolean
seahorse_gkr_item_commands_delete_objects (SeahorseCommands* commands,
                                           GList* objects)
{
	GCancellable *cancellable;
	GtkWidget *parent;
	gchar *prompt;
	gboolean ret;
	guint num;

	num = g_list_length (objects);
	if (num == 0)
		return TRUE;

	if (num == 1) {
		prompt = g_strdup_printf (_ ("Are you sure you want to delete the password '%s'?"), 
		                          seahorse_object_get_label (SEAHORSE_OBJECT (objects->data)));
	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to delete %d password?", 
		                                    "Are you sure you want to delete %d passwords?", 
		                                    num), num);
	}
	
	parent = GTK_WIDGET (seahorse_commands_get_window (commands));
	ret = seahorse_util_prompt_delete (prompt, parent);

	if (ret) {
		cancellable = g_cancellable_new ();
		seahorse_gkr_delete_async (objects, cancellable, on_delete_objects, g_object_ref (commands));
		seahorse_progress_show (cancellable, ngettext ("Deleting item", "Deleting items", num), TRUE);
		g_object_unref (cancellable);
	}

	g_free (prompt);
	return ret;
}

static void
seahorse_gkr_item_commands_constructed (GObject *obj)
{
	SeahorseCommands *commands = SEAHORSE_COMMANDS (obj);

	G_OBJECT_CLASS (seahorse_gkr_item_commands_parent_class)->constructed (obj);

	seahorse_viewer_register_commands (seahorse_commands_get_viewer (commands),
	                                   &commands_predicate, commands);
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

	gobject_class->constructed = seahorse_gkr_item_commands_constructed;

	cmd_class->show_properties = seahorse_gkr_item_commands_show_properties;
	cmd_class->delete_objects = seahorse_gkr_item_commands_delete_objects;

	/* Setup the predicate that matches our commands */
	commands_predicate.type = SEAHORSE_TYPE_GKR_ITEM;

	/* Register this class as a commands */
	seahorse_registry_register_type (seahorse_registry_get (),
	                                 SEAHORSE_TYPE_GKR_ITEM_COMMANDS,
	                                 "commands", NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

