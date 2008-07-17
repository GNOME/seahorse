/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

/**
 * SeahorseServerSoruce: A base class for key sources that retrieve keys
 * from remote key servers.
 * 
 * - Derived from SeahorseSource.
 * - Also includes functions for parsing keyserver URIs and mapping them
 *   to the appropriate key sources (such as SeahorseHKPSource)
 * - There's some GPGME specific stuff in here that may eventually need to be 
 *   moved elsewhere.
 * 
 * Properties:
 *   key-type: (GQuark) The type of keys generated (ie: SKEY_PGP)
 *   location: (gchar*) The location of keys from this key source (ie: SEAHORSE_LOCATION_REMOTE)
 *   key-server: (gchar*) The host:port of the keyserver to search.
 *   uri: (gchar*) Only for remote key sources. The full URI of the keyserver 
 *        being used. 
 */
 
#ifndef __SEAHORSE_SERVER_SOURCE_H__
#define __SEAHORSE_SERVER_SOURCE_H__

#include "seahorse-source.h"
#include "seahorse-operation.h"

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
    SeahorseSource parent;
    
    /*< private >*/
    SeahorseServerSourcePrivate *priv;
};

struct _SeahorseServerSourceClass {
    SeahorseSourceClass parent_class;
};

GType        seahorse_server_source_get_type         (void);

void         seahorse_server_source_take_operation   (SeahorseServerSource *ssrc,
                                                      SeahorseOperation *operation);

SeahorseServerSource* seahorse_server_source_new     (const gchar *uri);

#endif /* __SEAHORSE_SERVER_SOURCE_H__ */
