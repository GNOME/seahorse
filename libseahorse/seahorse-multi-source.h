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

#ifndef __SEAHORSE_MULTI_SOURCE_H__
#define __SEAHORSE_MULTI_SOURCE_H__

#include "seahorse-key-source.h"

#define SEAHORSE_TYPE_MULTI_SOURCE            (seahorse_multi_source_get_type ())
#define SEAHORSE_MULTI_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_MULTI_SOURCE, SeahorseMultiSource))
#define SEAHORSE_MULTI_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_MULTI_SOURCE, SeahorseMultiSourceClass))
#define SEAHORSE_IS_MULTI_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_MULTI_SOURCE))
#define SEAHORSE_IS_MULTI_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_MULTI_SOURCE))
#define SEAHORSE_MULTI_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_MULTI_SOURCE, SeahorseMultiSourceClass))

typedef struct _SeahorseMultiSource SeahorseMultiSource;
typedef struct _SeahorseMultiSourceClass SeahorseMultiSourceClass;

struct _SeahorseMultiSource {
    SeahorseKeySource parent;
    
    /*< private >*/
    GSList *sources;
};

struct _SeahorseMultiSourceClass {
    SeahorseKeySourceClass parent_class;
};

SeahorseMultiSource* seahorse_multi_source_new ();

GType       seahorse_multi_source_get_type       (void);

SeahorseKeySource*
            seahorse_multi_source_get_primary    (SeahorseMultiSource *msrc);

void        seahorse_multi_source_add            (SeahorseMultiSource *source,
                                                  SeahorseKeySource *sksrc,
                                                  gboolean important);

void        seahorse_multi_source_remove         (SeahorseMultiSource *source,
                                                  SeahorseKeySource *sksrc);

#endif /* __SEAHORSE_MULTI_SOURCE_H__ */
