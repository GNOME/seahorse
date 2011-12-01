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

#ifndef __SEAHORSE_GKR_OPERATION_H__
#define __SEAHORSE_GKR_OPERATION_H__

#include "seahorse-gkr-item.h"
#include "seahorse-gkr-keyring.h"

gboolean      seahorse_gkr_propagate_error           (GnomeKeyringResult result,
                                                      GError **err);

void          seahorse_gkr_update_description_async  (SeahorseGkrItem *item,
                                                      const gchar *description,
                                                      GCancellable *cancellable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data);

gboolean      seahorse_gkr_update_description_finish (SeahorseGkrItem *item,
                                                      GAsyncResult *result,
                                                      GError **error);

void          seahorse_gkr_update_secret_async       (SeahorseGkrItem *item,
                                                      const gchar *secret,
                                                      GCancellable *cancellable,
                                                      GAsyncReadyCallback callback,
                                                      gpointer user_data);

gboolean      seahorse_gkr_update_secret_finish      (SeahorseGkrItem *item,
                                                      GAsyncResult *result,
                                                      GError **error);

#endif /* __SEAHORSE_GKR_OPERATION_H__ */
