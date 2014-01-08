/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#ifndef __SEAHORSE_PREDICATE_H__
#define __SEAHORSE_PREDICATE_H__

#include "seahorse-object.h"

#include <glib-object.h>

typedef gboolean (*SeahorsePredicateFunc) (GObject *obj,
                                           void *user_data);

typedef struct _SeahorsePredicate SeahorsePredicate;

struct _SeahorsePredicate {
	GType type;
	SeahorseUsage usage;
	SeahorseFlags flags;
	SeahorseFlags nflags;
	SeahorsePredicateFunc custom;
	gpointer custom_target;
};

gboolean           seahorse_predicate_match         (SeahorsePredicate *pred,
                                                     GObject *obj);

#endif /* __SEAHORSE_PREDICATE_H__ */
