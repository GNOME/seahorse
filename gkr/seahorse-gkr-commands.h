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

#ifndef __SEAHORSE_GKR_COMMANDS_H__
#define __SEAHORSE_GKR_COMMANDS_H__

#include "seahorse-commands.h"

#include <glib-object.h>

#define SEAHORSE_TYPE_GKR_COMMANDS               (seahorse_gkr_commands_get_type ())
#define SEAHORSE_GKR_COMMANDS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_COMMANDS, SeahorseGkrCommands))
#define SEAHORSE_GKR_COMMANDS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_COMMANDS, SeahorseGkrCommandsClass))
#define SEAHORSE_IS_GKR_COMMANDS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_COMMANDS))
#define SEAHORSE_IS_GKR_COMMANDS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_COMMANDS))
#define SEAHORSE_GKR_COMMANDS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_COMMANDS, SeahorseGkrCommandsClass))

typedef struct _SeahorseGkrCommands SeahorseGkrCommands;
typedef struct _SeahorseGkrCommandsClass SeahorseGkrCommandsClass;
typedef struct _SeahorseGkrCommandsPrivate SeahorseGkrCommandsPrivate;
    
struct _SeahorseGkrCommands {
	SeahorseCommands parent;
};

struct _SeahorseGkrCommandsClass {
	SeahorseCommandsClass parent_class;
};

GType                  seahorse_gkr_commands_get_type               (void);

SeahorseGkrCommands*   seahorse_gkr_commands_new                    (void);

#endif /* __SEAHORSE_GKR_COMMANDS_H__ */
