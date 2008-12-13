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

#ifndef __SEAHORSE_SSH_COMMANDS_H__
#define __SEAHORSE_SSH_COMMANDS_H__

#include <glib.h>
#include <glib-object.h>

#include "seahorse-commands.h"
#include "seahorse-object.h"
#include <seahorse-operation.h>


#define SEAHORSE_TYPE_SSH_COMMANDS                 (seahorse_ssh_commands_get_type ())
#define SEAHORSE_SSH_COMMANDS(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_COMMANDS, SeahorseSshCommands))
#define SEAHORSE_SSH_COMMANDS_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_COMMANDS, SeahorseSshCommandsClass))
#define SEAHORSE_IS_SSH_COMMANDS(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_COMMANDS))
#define SEAHORSE_IS_SSH_COMMANDS_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_COMMANDS))
#define SEAHORSE_SSH_COMMANDS_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_COMMANDS, SeahorseSshCommandsClass))

typedef struct _SeahorseSshCommands SeahorseSshCommands;
typedef struct _SeahorseSshCommandsClass SeahorseSshCommandsClass;
typedef struct _SeahorseSshCommandsPrivate SeahorseSshCommandsPrivate;

struct _SeahorseSshCommands {
	SeahorseCommands parent_instance;
	SeahorseSshCommandsPrivate *pv;
};

struct _SeahorseSshCommandsClass {
	SeahorseCommandsClass parent_class;
};

GType                         seahorse_ssh_commands_get_type   (void);

SeahorseSshCommands*          seahorse_ssh_commands_new        (void);


#endif
