/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_TRANSFER_H__
#define __SEAHORSE_TRANSFER_H__

#include "seahorse-common.h"

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
