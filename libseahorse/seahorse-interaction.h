/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_INTERACTION_H__
#define __SEAHORSE_INTERACTION_H__

#include <gtk/gtk.h>

#include "seahorse-object.h"

#define SEAHORSE_TYPE_INTERACTION                  (seahorse_interaction_get_type ())
#define SEAHORSE_INTERACTION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_INTERACTION, SeahorseInteraction))
#define SEAHORSE_INTERACTION_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_INTERACTION, SeahorseInteractionClass))
#define SEAHORSE_IS_INTERACTION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_INTERACTION))
#define SEAHORSE_IS_INTERACTION_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_INTERACTION))
#define SEAHORSE_INTERACTION_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_INTERACTION, SeahorseInteractionClass))

typedef struct _SeahorseInteraction SeahorseInteraction;
typedef struct _SeahorseInteractionClass SeahorseInteractionClass;
typedef struct _SeahorseInteractionPrivate SeahorseInteractionPrivate;

struct _SeahorseInteraction {
	GTlsInteraction parent_instance;
	SeahorseInteractionPrivate *pv;
};

struct _SeahorseInteractionClass {
	GTlsInteractionClass parent_class;
};

GType                 seahorse_interaction_get_type                 (void);

GTlsInteraction *     seahorse_interaction_new                      (GtkWindow *parent);

GtkWindow *           seahorse_interaction_get_parent               (SeahorseInteraction *self);

void                  seahorse_interaction_set_parent               (SeahorseInteraction *self,
                                                                     GtkWindow *parent);

#endif
