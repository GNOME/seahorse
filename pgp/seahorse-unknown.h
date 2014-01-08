/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_UNKNOWN_H__
#define __SEAHORSE_UNKNOWN_H__

#include <gtk/gtk.h>

#include "seahorse-object.h"
#include "seahorse-unknown-source.h"

#define SEAHORSE_TYPE_UNKNOWN            (seahorse_unknown_get_type ())
#define SEAHORSE_UNKNOWN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_UNKNOWN, SeahorseUnknown))
#define SEAHORSE_UNKNOWN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_UNKNOWN, SeahorseUnknownClass))
#define SEAHORSE_IS_UNKNOWN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_UNKNOWN))
#define SEAHORSE_IS_UNKNOWN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_UNKNOWN))
#define SEAHORSE_UNKNOWN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_UNKNOWN, SeahorseUnknownClass))

typedef struct _SeahorseUnknown SeahorseUnknown;
typedef struct _SeahorseUnknownClass SeahorseUnknownClass;

struct _SeahorseUnknown {
    SeahorseObject parent;

    /*< public >*/
    gchar *display;
};

struct _SeahorseUnknownClass {
    SeahorseObjectClass            parent_class;
};

GType                seahorse_unknown_get_type         (void);

SeahorseUnknown*     seahorse_unknown_new              (SeahorseUnknownSource *usrc,
                                                        const gchar *keyid,
                                                        const gchar *display);

#endif /* __SEAHORSE_UNKNOWN_H__ */
