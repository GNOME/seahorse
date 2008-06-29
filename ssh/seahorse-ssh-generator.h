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

#ifndef __SEAHORSE_SSH_GENERATOR_H__
#define __SEAHORSE_SSH_GENERATOR_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-generator.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS


#define SEAHORSE_SSH_TYPE_GENERATOR (seahorse_ssh_generator_get_type ())
#define SEAHORSE_SSH_GENERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_SSH_TYPE_GENERATOR, SeahorseSSHGenerator))
#define SEAHORSE_SSH_GENERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_SSH_TYPE_GENERATOR, SeahorseSSHGeneratorClass))
#define SEAHORSE_SSH_IS_GENERATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_SSH_TYPE_GENERATOR))
#define SEAHORSE_SSH_IS_GENERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_SSH_TYPE_GENERATOR))
#define SEAHORSE_SSH_GENERATOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_SSH_TYPE_GENERATOR, SeahorseSSHGeneratorClass))

typedef struct _SeahorseSSHGenerator SeahorseSSHGenerator;
typedef struct _SeahorseSSHGeneratorClass SeahorseSSHGeneratorClass;
typedef struct _SeahorseSSHGeneratorPrivate SeahorseSSHGeneratorPrivate;

struct _SeahorseSSHGenerator {
	SeahorseGenerator parent_instance;
	SeahorseSSHGeneratorPrivate * priv;
};

struct _SeahorseSSHGeneratorClass {
	SeahorseGeneratorClass parent_class;
};


SeahorseSSHGenerator* seahorse_ssh_generator_new (void);
GType seahorse_ssh_generator_get_type (void);


G_END_DECLS

#endif
