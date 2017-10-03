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

#include "config.h"

#include "seahorse-predicate.h"

#include "string.h"

#include <glib/gi18n.h>

/**
 * seahorse_predicate_match:
 * @self: the object to test
 * @obj: The predicate to match
 *
 * matches a seahorse object and a predicate
 *
 * Returns: FALSE if predicate does not match the #SeahorseObject, TRUE else
 */
gboolean
seahorse_predicate_match (SeahorsePredicate *pred,
                          GObject* obj)
{

	g_return_val_if_fail (G_IS_OBJECT (obj), FALSE);

	/* Check all the fields */
	if (pred->type != 0 && !G_TYPE_CHECK_INSTANCE_TYPE (obj, pred->type))
		return FALSE;

	if (pred->usage != 0) {
		SeahorseUsage usage = 0;
		g_object_get (obj, "usage", &usage, NULL);
		if (pred->usage != usage)
			return FALSE;
	}

	if (pred->flags != 0 || pred->nflags != 0) {
		SeahorseFlags flags = 0;
		g_object_get (obj, "object-flags", &flags, NULL);
		if (pred->flags != 0 && (pred->flags & flags) == 0)
			return FALSE;
		if (pred->nflags != 0 && (pred->nflags & flags) != 0)
			return FALSE;
	}

	/* And any custom stuff */
	if (pred->custom != NULL && !pred->custom (obj, pred->custom_target))
		return FALSE;

	return TRUE;
}
