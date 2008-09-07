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

#ifndef __SEAHORSE_COMMANDS_H__
#define __SEAHORSE_COMMANDS_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-operation.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <seahorse-object.h>
#include <seahorse-view.h>

G_BEGIN_DECLS


#define SEAHORSE_TYPE_COMMANDS (seahorse_commands_get_type ())
#define SEAHORSE_COMMANDS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_COMMANDS, SeahorseCommands))
#define SEAHORSE_COMMANDS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_COMMANDS, SeahorseCommandsClass))
#define SEAHORSE_IS_COMMANDS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_COMMANDS))
#define SEAHORSE_IS_COMMANDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_COMMANDS))
#define SEAHORSE_COMMANDS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_COMMANDS, SeahorseCommandsClass))

typedef struct _SeahorseCommands SeahorseCommands;
typedef struct _SeahorseCommandsClass SeahorseCommandsClass;
typedef struct _SeahorseCommandsPrivate SeahorseCommandsPrivate;

struct _SeahorseCommands {
	GObject parent_instance;
	SeahorseCommandsPrivate * priv;
};

struct _SeahorseCommandsClass {
	GObjectClass parent_class;
	void (*show_properties) (SeahorseCommands* self, SeahorseObject* obj);
	SeahorseOperation* (*delete_objects) (SeahorseCommands* self, GList* obj);
	GQuark (*get_ktype) (SeahorseCommands* self);
	GtkActionGroup* (*get_command_actions) (SeahorseCommands* self);
	const char* (*get_ui_definition) (SeahorseCommands* self);
};


void seahorse_commands_show_properties (SeahorseCommands* self, SeahorseObject* obj);
SeahorseOperation* seahorse_commands_delete_objects (SeahorseCommands* self, GList* obj);
SeahorseView* seahorse_commands_get_view (SeahorseCommands* self);
GQuark seahorse_commands_get_ktype (SeahorseCommands* self);
GtkActionGroup* seahorse_commands_get_command_actions (SeahorseCommands* self);
const char* seahorse_commands_get_ui_definition (SeahorseCommands* self);
GType seahorse_commands_get_type (void);


G_END_DECLS

#endif
