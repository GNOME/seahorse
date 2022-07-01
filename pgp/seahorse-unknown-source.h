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

#pragma once

#include "seahorse-common.h"
#include "seahorse-unknown.h"

#define SEAHORSE_TYPE_UNKNOWN_SOURCE (seahorse_unknown_source_get_type ())
G_DECLARE_FINAL_TYPE (SeahorseUnknownSource, seahorse_unknown_source,
                      SEAHORSE, UNKNOWN_SOURCE,
                      GObject)

SeahorseUnknownSource *   seahorse_unknown_source_new           (void);

SeahorseUnknown *         seahorse_unknown_source_add_object    (SeahorseUnknownSource *self,
                                                                 const char *keyid,
                                                                 GCancellable *cancellable);
