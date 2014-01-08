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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
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
