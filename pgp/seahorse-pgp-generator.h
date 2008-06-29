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

#ifndef __SEAHORSE_PGP_GENERATOR_H__
#define __SEAHORSE_PGP_GENERATOR_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-generator.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS


#define SEAHORSE_PGP_TYPE_GENERATOR (seahorse_pgp_generator_get_type ())
#define SEAHORSE_PGP_GENERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PGP_TYPE_GENERATOR, SeahorsePGPGenerator))
#define SEAHORSE_PGP_GENERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PGP_TYPE_GENERATOR, SeahorsePGPGeneratorClass))
#define SEAHORSE_PGP_IS_GENERATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PGP_TYPE_GENERATOR))
#define SEAHORSE_PGP_IS_GENERATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PGP_TYPE_GENERATOR))
#define SEAHORSE_PGP_GENERATOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PGP_TYPE_GENERATOR, SeahorsePGPGeneratorClass))

typedef struct _SeahorsePGPGenerator SeahorsePGPGenerator;
typedef struct _SeahorsePGPGeneratorClass SeahorsePGPGeneratorClass;
typedef struct _SeahorsePGPGeneratorPrivate SeahorsePGPGeneratorPrivate;

struct _SeahorsePGPGenerator {
	SeahorseGenerator parent_instance;
	SeahorsePGPGeneratorPrivate * priv;
};

struct _SeahorsePGPGeneratorClass {
	SeahorseGeneratorClass parent_class;
};


SeahorsePGPGenerator* seahorse_pgp_generator_new (void);
GType seahorse_pgp_generator_get_type (void);


G_END_DECLS

#endif
