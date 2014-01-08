/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include <glib.h>
#include <glib-object.h>

#ifndef SEAHORSEOBJECTLIST_H_
#define SEAHORSEOBJECTLIST_H_

#define    SEAHORSE_BOXED_OBJECT_LIST        (seahorse_object_list_type ())

GType      seahorse_object_list_type         (void);

GList*     seahorse_object_list_append       (GList *original, gpointer object);

GList*     seahorse_object_list_prepend      (GList *original, gpointer object);

GList*     seahorse_object_list_remove       (GList *original, gpointer object);

GList*     seahorse_object_list_copy         (GList *original);

void       seahorse_object_list_free         (gpointer list);

#endif /* SEAHORSEOBJECTLIST_H_ */
