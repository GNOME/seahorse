/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

/**
 * SeahorseServerSource: A base class for key sources that retrieve keys
 * from remote key servers.
 *
 * - Derived from SeahorseSource.
 * - Also includes functions for parsing keyserver URIs and mapping them
 *   to the appropriate key sources (such as SeahorseHKPSource)
 * - There's some GPGME specific stuff in here that may eventually need to be
 *   moved elsewhere.
 *
 * Properties:
 *   key-type: (GQuark) The type of keys generated (ie: SKEY_PGP)
 *   location: (gchar*) The location of keys from this key source (ie: SEAHORSE_LOCATION_REMOTE)
 *   key-server: (gchar*) The host:port of the keyserver to search.
 *   uri: (gchar*) Only for remote key sources. The full URI of the keyserver
 *        being used.
 */

#pragma once

#include <gcr/gcr.h>

#define SEAHORSE_TYPE_SERVER_SOURCE (seahorse_server_source_get_type ())
G_DECLARE_DERIVABLE_TYPE (SeahorseServerSource, seahorse_server_source,
                          SEAHORSE, SERVER_SOURCE,
                          GObject)

struct _SeahorseServerSourceClass {
    GObjectClass parent_class;

    void            (*import_async)          (SeahorseServerSource *source,
                                              GInputStream *input,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer user_data);

    GList *         (*import_finish)         (SeahorseServerSource *source,
                                              GAsyncResult *result,
                                              GError **error);

    void            (*export_async)          (SeahorseServerSource *source,
                                              const gchar **keyids,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer user_data);

    GBytes *        (*export_finish)         (SeahorseServerSource *source,
                                              GAsyncResult *result,
                                              GError **error);

    void            (*search_async)          (SeahorseServerSource *source,
                                              const char *match,
                                              GListStore *results,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer user_data);

    gboolean        (*search_finish)         (SeahorseServerSource *source,
                                              GAsyncResult *result,
                                              GError **error);
};

SeahorseServerSource*  seahorse_server_source_new              (const char *uri);

void                   seahorse_server_source_search_async     (SeahorseServerSource *self,
                                                                const char *match,
                                                                GListStore *results,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

gboolean               seahorse_server_source_search_finish    (SeahorseServerSource *self,
                                                                GAsyncResult *result,
                                                                GError **error);

void                   seahorse_server_source_import_async     (SeahorseServerSource *self,
                                                                GInputStream *input,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

GList *                seahorse_server_source_import_finish    (SeahorseServerSource *self,
                                                                GAsyncResult *result,
                                                                GError **error);

void                   seahorse_server_source_export_async     (SeahorseServerSource *self,
                                                                const gchar **keyids,
                                                                GCancellable *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer user_data);

GBytes *               seahorse_server_source_export_finish    (SeahorseServerSource *self,
                                                                GAsyncResult *result,
                                                                GError **error);
