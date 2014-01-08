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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef SEAHORSE_PGP_BACKEND_H_
#define SEAHORSE_PGP_BACKEND_H_

#include <glib.h>
#include <glib-object.h>

#include <gcr/gcr.h>

#include "seahorse-pgp.h"
#include "seahorse-discovery.h"
#include "seahorse-gpgme-keyring.h"
#include "seahorse-pgp-key.h"
#include "seahorse-server-source.h"

G_BEGIN_DECLS

#define SEAHORSE_TYPE_PGP_BACKEND            (seahorse_pgp_backend_get_type ())
#define SEAHORSE_PGP_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_BACKEND, SeahorsePgpBackend))
#define SEAHORSE_PGP_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_BACKEND, SeahorsePgpBackendClass))
#define SEAHORSE_IS_PGP_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_BACKEND))
#define SEAHORSE_IS_PGP_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_BACKEND))
#define SEAHORSE_PGP_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_BACKEND, SeahorsePgpBackendClass))

typedef struct _SeahorsePgpBackend SeahorsePgpBackend;
typedef struct _SeahorsePgpBackendClass SeahorsePgpBackendClass;

GType                  seahorse_pgp_backend_get_type             (void) G_GNUC_CONST;

SeahorsePgpBackend *   seahorse_pgp_backend_get                  (void);

SeahorseGpgmeKeyring * seahorse_pgp_backend_get_default_keyring  (SeahorsePgpBackend *self);

SeahorsePgpKey *       seahorse_pgp_backend_get_default_key      (SeahorsePgpBackend *self);

#ifdef WITH_KEYSERVER

SeahorseDiscovery *    seahorse_pgp_backend_get_discovery        (SeahorsePgpBackend *self);

SeahorseServerSource * seahorse_pgp_backend_lookup_remote        (SeahorsePgpBackend *self,
                                                                  const gchar *uri);

void                   seahorse_pgp_backend_add_remote           (SeahorsePgpBackend *self,
                                                                  const gchar *uri,
                                                                  SeahorseServerSource *source);

void                   seahorse_pgp_backend_remove_remote        (SeahorsePgpBackend *self,
                                                                  const gchar *uri);

void                   seahorse_pgp_backend_search_remote_async  (SeahorsePgpBackend *self,
                                                                  const gchar *search,
                                                                  GcrSimpleCollection *results,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

gboolean               seahorse_pgp_backend_search_remote_finish (SeahorsePgpBackend *self,
                                                                  GAsyncResult *result,
                                                                  GError **error);

void                   seahorse_pgp_backend_transfer_async       (SeahorsePgpBackend *self,
                                                                  GList *keys,
                                                                  SeahorsePlace *to,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer user_data);

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


G_END_DECLS

#endif /*SEAHORSE_PGP_BACKEND_H_*/
