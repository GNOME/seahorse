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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SEAHORSE_TRANSFER_H__
#define __SEAHORSE_TRANSFER_H__

#include "seahorse-place.h"

#include "seahorse-server-source.h"

void            seahorse_transfer_keyids_async  (SeahorseServerSource *from,
                                                 SeahorsePlace *to,
                                                 const gchar **keyids,
                                                 GCancellable *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer user_data);

void            seahorse_transfer_keys_async    (SeahorsePlace *from,
                                                 SeahorsePlace *to,
                                                 GList *keys,
                                                 GCancellable *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer user_data);

gboolean        seahorse_transfer_finish        (GAsyncResult *result,
                                                 GError **error);

#endif /* __SEAHORSE_TRANSFER_H__ */
