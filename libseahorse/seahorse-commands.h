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

#ifndef __SEAHORSE_COMMANDS_H__
#define __SEAHORSE_COMMANDS_H__

#include <glib.h>
#include <glib-object.h>

#include "seahorse-object.h"
#include "seahorse-view.h"

#define SEAHORSE_TYPE_COMMANDS                  (seahorse_commands_get_type ())
#define SEAHORSE_COMMANDS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_COMMANDS, SeahorseCommands))
#define SEAHORSE_COMMANDS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_COMMANDS, SeahorseCommandsClass))
#define SEAHORSE_IS_COMMANDS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_COMMANDS))
#define SEAHORSE_IS_COMMANDS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_COMMANDS))
#define SEAHORSE_COMMANDS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_COMMANDS, SeahorseCommandsClass))

typedef struct _SeahorseCommandsClass SeahorseCommandsClass;
typedef struct _SeahorseCommandsPrivate SeahorseCommandsPrivate;

struct _SeahorseCommands {
	GObject parent_instance;
	SeahorseCommandsPrivate *pv;
};

struct _SeahorseCommandsClass {
	GObjectClass parent_class;
	
	/* Virtual methods */
	
	void        (*show_properties)         (SeahorseCommands *self,
	                                        SeahorseObject *obj);

	gboolean    (*delete_objects)          (SeahorseCommands *self,
	                                        GList *obj);
};

GType                 seahorse_commands_get_type                 (void);

void                  seahorse_commands_show_properties          (SeahorseCommands *self, SeahorseObject *obj);

gboolean              seahorse_commands_delete_objects           (SeahorseCommands *self,
                                                                  GList *obj);

SeahorseView *        seahorse_commands_get_view                 (SeahorseCommands *self);

GtkWindow *           seahorse_commands_get_window               (SeahorseCommands *self);

GtkActionGroup *      seahorse_commands_get_command_actions      (SeahorseCommands *self);

const char *          seahorse_commands_get_ui_definition        (SeahorseCommands *self);

#endif
