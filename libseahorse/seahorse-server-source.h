/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SEAHORSE_SERVER_SOURCE_H__
#define __SEAHORSE_SERVER_SOURCE_H__

#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_SERVER_SOURCE            (seahorse_server_source_get_type ())
#define SEAHORSE_SERVER_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SERVER_SOURCE, SeahorseServerSource))
#define SEAHORSE_SERVER_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SERVER_SOURCE, SeahorseServerSourceClass))
#define SEAHORSE_IS_SERVER_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SERVER_SOURCE))
#define SEAHORSE_IS_SERVER_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SERVER_SOURCE))
#define SEAHORSE_SERVER_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SERVER_SOURCE, SeahorseServerSourceClass))

typedef struct _SeahorseServerSource SeahorseServerSource;
typedef struct _SeahorseServerSourceClass SeahorseServerSourceClass;
typedef struct _SeahorseServerSourcePrivate SeahorseServerSourcePrivate;

struct _SeahorseServerSource {
    SeahorseKeySource parent;
    
    /*< private >*/
    SeahorseServerSourcePrivate *priv;
};

struct _SeahorseServerSourceClass {
    SeahorseKeySourceClass parent_class;
};

SeahorseServerSource* seahorse_server_source_new     (SeahorseKeySource *sksrc, 
                                                      const gchar *server);

void             seahorse_server_source_search       (SeahorseServerSource *source,
                                                      const gchar *pattern);

#endif /* __SEAHORSE_SERVER_SOURCE_H__ */
