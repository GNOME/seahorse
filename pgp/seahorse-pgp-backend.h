/*
 * Seahorse
 *
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#pragma once

#include <glib.h>
#include <glib-object.h>

#include <gcr/gcr.h>

#include "seahorse-discovery.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-pgp-key.h"
#include "seahorse-server-source.h"

G_BEGIN_DECLS

#define SEAHORSE_PGP_NAME      "openpgp"
#define SEAHORSE_PGP_TYPE_STR  SEAHORSE_PGP_NAME
#define SEAHORSE_PGP           g_quark_from_string (SEAHORSE_PGP_NAME)

void       seahorse_pgp_backend_initialize    (const char *gpg_homedir);

#define SEAHORSE_PGP_TYPE_BACKEND (seahorse_pgp_backend_get_type ())
G_DECLARE_FINAL_TYPE (SeahorsePgpBackend, seahorse_pgp_backend, SEAHORSE_PGP, BACKEND, GObject)

SeahorsePgpBackend *   seahorse_pgp_backend_get                  (void);

const char *           seahorse_pgp_backend_get_gpg_homedir      (SeahorsePgpBackend *self);

SeahorseGpgmeKeyring * seahorse_pgp_backend_get_default_keyring  (SeahorsePgpBackend *self);

SeahorsePgpKey *       seahorse_pgp_backend_get_default_key      (SeahorsePgpBackend *self);

#ifdef WITH_KEYSERVER

SeahorseDiscovery *    seahorse_pgp_backend_get_discovery        (SeahorsePgpBackend *self);

GListModel *           seahorse_pgp_backend_get_remotes          (SeahorsePgpBackend *self);

SeahorseServerSource * seahorse_pgp_backend_lookup_remote        (SeahorsePgpBackend *self,
                                                                  const gchar *uri);

void                   seahorse_pgp_backend_add_remote           (SeahorsePgpBackend *self,
                                                                  const char         *uri,
                                                                  gboolean            persist);

void                   seahorse_pgp_backend_remove_remote        (SeahorsePgpBackend *self,
                                                                  const char         *uri);

void                   seahorse_pgp_backend_search_remote_async  (SeahorsePgpBackend *self,
                                                                  const char *search,
                                                                  GListStore *results,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

gboolean               seahorse_pgp_backend_search_remote_finish (SeahorsePgpBackend *self,
                                                                  GAsyncResult *result,
                                                                  GError **error);

void                   seahorse_pgp_backend_transfer_async       (SeahorsePgpBackend  *self,
                                                                  GListModel          *keys,
                                                                  SeahorsePlace       *to,
                                                                  GCancellable        *cancellable,
                                                                  GAsyncReadyCallback  callback,
                                                                  void                *user_data);

gboolean               seahorse_pgp_backend_transfer_finish      (SeahorsePgpBackend *self,
                                                                  GAsyncResult *result,
                                                                  GError **error);

void                   seahorse_pgp_backend_retrieve_async       (SeahorsePgpBackend *self,
                                                                  const gchar **keyids,
                                                                  SeahorsePlace *to,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

gboolean               seahorse_pgp_backend_retrieve_finish      (SeahorsePgpBackend *self,
                                                                  GAsyncResult *result,
                                                                  GError **error);
#endif /* WITH_KEYSERVER */

GList*                 seahorse_pgp_backend_discover_keys        (SeahorsePgpBackend *self,
                                                                  const gchar **keyids,
                                                                  GCancellable *cancellable);

SeahorsePgpKey *       seahorse_pgp_backend_create_key_for_parsed (SeahorsePgpBackend *self,
                                                                   GcrParsed *parsed);


G_END_DECLS
