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
    SeahorseKeySource parent;
    
    /*< private >*/
    SeahorseServerSourcePrivate *priv;
};

struct _SeahorseServerSourceClass {
    SeahorseKeySourceClass parent_class;
};

GType        seahorse_server_source_get_type         (void);

void         seahorse_server_source_add_key          (SeahorseServerSource *ssrc,
                                                      gpgme_key_t key);

void         seahorse_server_source_set_operation    (SeahorseServerSource *ssrc,
                                                      SeahorseOperation *operation);

SeahorseServerSource* seahorse_server_source_new     (SeahorseKeySource *locsrc,
                                                      const gchar *server,
                                                      const gchar *pattern);

GSList*     seahorse_server_source_get_types         ();

GSList*     seahorse_server_source_get_descriptions  ();

gboolean    seahorse_server_source_valid_uri         (const gchar *uri);

GSList*     seahorse_server_source_parse_keyservers  (GSList *keyservers);

GSList*		seahorse_server_source_purge_keyservers	 (GSList *keyservers);

#endif /* __SEAHORSE_SERVER_SOURCE_H__ */
