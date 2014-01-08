/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_SSH_EXPORTER_H__
#define __SEAHORSE_SSH_EXPORTER_H__

#include <glib-object.h>

#include "seahorse-common.h"

#define SEAHORSE_TYPE_SSH_EXPORTER            (seahorse_ssh_exporter_get_type ())
#define SEAHORSE_SSH_EXPORTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_EXPORTER, SeahorseSshExporter))
#define SEAHORSE_IS_SSH_EXPORTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_EXPORTER))

typedef struct _SeahorseSshExporter SeahorseSshExporter;

GType                     seahorse_ssh_exporter_get_type     (void) G_GNUC_CONST;

SeahorseExporter *        seahorse_ssh_exporter_new          (GObject *object,
                                                              gboolean secret);

#endif /* __SEAHORSE_SSH_EXPORTER_H__ */
