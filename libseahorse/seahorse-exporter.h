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
 * 59 Temple Exporter, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_EXPORTER_H__
#define __SEAHORSE_EXPORTER_H__

#include "seahorse-types.h"

#include <gio/gio.h>

#include <gtk/gtk.h>

typedef enum {
	SEAHORSE_EXPORTER_ANY,
	SEAHORSE_EXPORTER_TEXTUAL
} SeahorseExporterType;

#define SEAHORSE_TYPE_EXPORTER                (seahorse_exporter_get_type ())
#define SEAHORSE_EXPORTER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_EXPORTER, SeahorseExporter))
#define SEAHORSE_IS_EXPORTER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_EXPORTER))
#define SEAHORSE_EXPORTER_GET_INTERFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SEAHORSE_TYPE_EXPORTER, SeahorseExporterIface))

typedef struct _SeahorseExporter SeahorseExporter;
typedef struct _SeahorseExporterIface SeahorseExporterIface;

struct _SeahorseExporterIface {
	GTypeInterface parent;

	/* virtual methods ------------------------------------------------- */

	GList *         (* get_objects)              (SeahorseExporter *exporter);

	gboolean        (* add_object)               (SeahorseExporter *exporter,
	                                              GObject *object);

	void            (* export_async)             (SeahorseExporter *exporter,
	                                              GCancellable *cancellable,
	                                              GAsyncReadyCallback callback,
	                                              gpointer user_data);

	gpointer        (* export_finish)            (SeahorseExporter *exporter,
	                                              GAsyncResult *result,
	                                              gsize *size,
	                                              GError **error);
};

GType            seahorse_exporter_get_type             (void) G_GNUC_CONST;

gchar *          seahorse_exporter_get_filename         (SeahorseExporter *exporter);

gchar *          seahorse_exporter_get_content_type     (SeahorseExporter *exporter);

GtkFileFilter *  seahorse_exporter_get_file_filter      (SeahorseExporter *exporter);

GList *          seahorse_exporter_get_objects          (SeahorseExporter *exporter);

gboolean         seahorse_exporter_add_object           (SeahorseExporter *exporter,
                                                         GObject *object);

void             seahorse_exporter_export_async         (SeahorseExporter *exporter,
                                                         GCancellable *cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer user_data);

gpointer         seahorse_exporter_export_finish        (SeahorseExporter *exporter,
                                                         GAsyncResult *result,
                                                         gsize *size,
                                                         GError **error);

void            seahorse_exporter_export_to_file_async  (SeahorseExporter *exporter,
                                                         GFile *file,
                                                         gboolean overwrite,
                                                         GCancellable *cancellable,
                                                         GAsyncReadyCallback callback,
                                                         gpointer user_data);

gboolean        seahorse_exporter_export_to_file_finish (SeahorseExporter *exporter,
                                                         GAsyncResult *result,
                                                         GError **error);

#endif /* __SEAHORSE_EXPORTER_H__ */
