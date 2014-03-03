/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __SEAHORSE_UNKNOWN_SOURCE_H__
#define __SEAHORSE_UNKNOWN_SOURCE_H__

#include "libseahorse/seahorse-object.h"

#define SEAHORSE_TYPE_UNKNOWN_SOURCE            (seahorse_unknown_source_get_type ())
#define SEAHORSE_UNKNOWN_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_UNKNOWN_SOURCE, SeahorseUnknownSource))
#define SEAHORSE_UNKNOWN_SOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_UNKNOWN_SOURCE, SeahorseUnknownSourceClass))
#define SEAHORSE_IS_UNKNOWN_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_UNKNOWN_SOURCE))
#define SEAHORSE_IS_UNKNOWN_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_UNKNOWN_SOURCE))
#define SEAHORSE_UNKNOWN_SOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_UNKNOWN_SOURCE, SeahorseUnknownSourceClass))

typedef struct _SeahorseUnknownSource SeahorseUnknownSource;
typedef struct _SeahorseUnknownSourceClass SeahorseUnknownSourceClass;
typedef struct _SeahorseUnknownSourcePrivate SeahorseUnknownSourcePrivate;

GType                    seahorse_unknown_source_get_type      (void);

SeahorseUnknownSource*   seahorse_unknown_source_new           (void);

SeahorseObject*          seahorse_unknown_source_add_object    (SeahorseUnknownSource *self,
                                                                const gchar *keyid,
                                                                GCancellable *cancellable);

#endif /* __SEAHORSE_UNKNOWN_SOURCE_H__ */
