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

#ifndef __SEAHORSE_PKCS11_COMMANDS_H__
#define __SEAHORSE_PKCS11_COMMANDS_H__

#include "seahorse-commands.h"

#include <glib-object.h>

#define SEAHORSE_PKCS11_TYPE_COMMANDS               (seahorse_pkcs11_commands_get_type ())
#define SEAHORSE_PKCS11_COMMANDS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_TYPE_COMMANDS, SeahorsePkcs11Commands))
#define SEAHORSE_PKCS11_COMMANDS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_TYPE_COMMANDS, SeahorsePkcs11CommandsClass))
#define SEAHORSE_PKCS11_IS_COMMANDS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_TYPE_COMMANDS))
#define SEAHORSE_PKCS11_IS_COMMANDS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_TYPE_COMMANDS))
#define SEAHORSE_PKCS11_COMMANDS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_TYPE_COMMANDS, SeahorsePkcs11CommandsClass))

typedef struct _SeahorsePkcs11Commands SeahorsePkcs11Commands;
typedef struct _SeahorsePkcs11CommandsClass SeahorsePkcs11CommandsClass;
typedef struct _SeahorsePkcs11CommandsPrivate SeahorsePkcs11CommandsPrivate;
    
struct _SeahorsePkcs11Commands {
	SeahorseCommands parent;
	SeahorsePkcs11CommandsPrivate *pv;
};

struct _SeahorsePkcs11CommandsClass {
	SeahorseCommandsClass parent_class;
};

GType                        seahorse_pkcs11_commands_get_type               (void);

SeahorsePkcs11Commands*      seahorse_pkcs11_commands_new                    (void);

#endif /* __SEAHORSE_PKCS11_COMMANDS_H__ */
