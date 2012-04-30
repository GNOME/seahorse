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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef SEAHORSE_GKR_BACKEND_H_
#define SEAHORSE_GKR_BACKEND_H_

#include <glib.h>
#include <glib-object.h>

#include <gcr/gcr.h>

#include "seahorse-gkr-keyring.h"

G_BEGIN_DECLS

#define SEAHORSE_TYPE_GKR_BACKEND            (seahorse_gkr_backend_get_type ())
#define SEAHORSE_GKR_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_BACKEND, SeahorseGkrBackend))
#define SEAHORSE_GKR_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_BACKEND, SeahorseGkrBackendClass))
#define SEAHORSE_IS_GKR_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_BACKEND))
#define SEAHORSE_IS_GKR_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_BACKEND))
#define SEAHORSE_GKR_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_BACKEND, SeahorseGkrBackendClass))

typedef struct _SeahorseGkrBackend SeahorseGkrBackend;
typedef struct _SeahorseGkrBackendClass SeahorseGkrBackendClass;

GType                 seahorse_gkr_backend_get_type        (void) G_GNUC_CONST;

SeahorseGkrBackend *  seahorse_gkr_backend_get             (void);

SecretService *       seahorse_gkr_backend_get_service     (SeahorseGkrBackend *self);

GList *               seahorse_gkr_backend_get_keyrings    (SeahorseGkrBackend *self);

gboolean              seahorse_gkr_backend_has_alias       (SeahorseGkrBackend *self,
                                                            const gchar *alias,
                                                            SeahorseGkrKeyring *keyring);

void                  seahorse_gkr_backend_load_async      (SeahorseGkrBackend *self,
                                                            GCancellable *cancellable,
                                                            GAsyncReadyCallback callback,
                                                            gpointer user_data);

gboolean              seahorse_gkr_backend_load_finish     (SeahorseGkrBackend *self,
                                                            GAsyncResult *result,
                                                            GError **error);

G_END_DECLS

#endif /*SEAHORSE_GKR_BACKEND_H_*/
