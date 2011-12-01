/*
 * Seahorse
 *
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
 * 59 Temple Exportable, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_EXPORTABLE_H__
#define __SEAHORSE_EXPORTABLE_H__

#include "seahorse-exporter.h"

#include <gio/gio.h>

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_EXPORTABLE                (seahorse_exportable_get_type ())
#define SEAHORSE_EXPORTABLE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_EXPORTABLE, SeahorseExportable))
#define SEAHORSE_IS_EXPORTABLE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_EXPORTABLE))
#define SEAHORSE_EXPORTABLE_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_EXPORTABLE, SeahorseExportableIface))

typedef struct _SeahorseExportable SeahorseExportable;
typedef struct _SeahorseExportableIface SeahorseExportableIface;

struct _SeahorseExportableIface {
	GTypeInterface parent;

	GList *     (* create_exporters)      (SeahorseExportable *exportable,
	                                       SeahorseExporterType type);
};

GType       seahorse_exportable_get_type                    (void) G_GNUC_CONST;

GList *     seahorse_exportable_create_exporters            (SeahorseExportable *exportable,
                                                             SeahorseExporterType type);

gboolean    seahorse_exportable_can_export                  (gpointer object);

guint       seahorse_exportable_export_to_directory_wait    (GList *objects,
                                                             const gchar *directory,
                                                             GError **error);

guint       seahorse_exportable_export_to_text_wait         (GList *objects,
                                                             gpointer *data,
                                                             gsize *size,
                                                             GError **error);

guint       seahorse_exportable_export_to_prompt_wait       (GList *objects,
                                                             GtkWindow *parent,
                                                             GError **error);

gboolean    seahorse_exportable_prompt                      (GList *exporters,
                                                             GtkWindow *parent,
                                                             gchar **directory,
                                                             GFile **file,
                                                             SeahorseExporter **exporter);

#endif /* __SEAHORSE_EXPORTABLE_H__ */
