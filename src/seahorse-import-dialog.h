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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __SEAHORSE_IMPORT_DIALOG_H__
#define __SEAHORSE_IMPORT_DIALOG_H__

#include <gcr/gcr.h>

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_IMPORT_DIALOG             (seahorse_import_dialog_get_type ())
#define SEAHORSE_IMPORT_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_IMPORT_DIALOG, SeahorseImportDialog))
#define SEAHORSE_IMPORT_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_IMPORT_DIALOG, SeahorseImportDialogClass))
#define SEAHORSE_IS_IMPORT_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_IMPORT_DIALOG))
#define SEAHORSE_IS_IMPORT_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_IMPORT_DIALOG))
#define SEAHORSE_IMPORT_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_IMPORT_DIALOG_KEY, SeahorseImportDialogClass))

typedef struct _SeahorseImportDialog SeahorseImportDialog;
typedef struct _SeahorseImportDialogClass SeahorseImportDialogClass;

GType               seahorse_import_dialog_get_type           (void);

GtkDialog *         seahorse_import_dialog_new                (GtkWindow *parent);

void                seahorse_import_dialog_add_uris           (SeahorseImportDialog *self,
                                                               const gchar **uris);

void                seahorse_import_dialog_add_text           (SeahorseImportDialog *self,
                                                               const gchar *display_name,
                                                               const gchar *text);

#endif /* __SEAHORSE_IMPORT_DIALOG_H__ */
