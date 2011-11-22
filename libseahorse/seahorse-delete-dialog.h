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

#ifndef __SEAHORSE_DELETE_DIALOG_H__
#define __SEAHORSE_DELETE_DIALOG_H__

#include <glib-object.h>

#include "seahorse-widget.h"

#define SEAHORSE_DELETE_DIALOG_MENU_OBJECT   "ObjectPopup"

#define SEAHORSE_TYPE_DELETE_DIALOG               (seahorse_delete_dialog_get_type ())
#define SEAHORSE_DELETE_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_DELETE_DIALOG, SeahorseDeleteDialog))
#define SEAHORSE_IS_DELETE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_DELETE_DIALOG))

typedef struct _SeahorseDeleteDialog SeahorseDeleteDialog;

GType               seahorse_delete_dialog_get_type                 (void) G_GNUC_CONST;

GtkDialog *         seahorse_delete_dialog_new                      (GtkWindow *parent,
                                                                     const gchar *format,
                                                                     ...) G_GNUC_PRINTF (2, 3);

const gchar *       seahorse_delete_dialog_get_check_label          (SeahorseDeleteDialog *self);

void                seahorse_delete_dialog_set_check_label          (SeahorseDeleteDialog *self,
                                                                     const gchar *label);

gboolean            seahorse_delete_dialog_get_check_value          (SeahorseDeleteDialog *self);

void                seahorse_delete_dialog_set_check_value          (SeahorseDeleteDialog *self,
                                                                     gboolean value);

gboolean            seahorse_delete_dialog_get_check_require        (SeahorseDeleteDialog *self);

void                seahorse_delete_dialog_set_check_require        (SeahorseDeleteDialog *self,
                                                                     gboolean value);

gboolean            seahorse_delete_dialog_prompt                   (GtkWindow *parent,
                                                                     const gchar *text);

#endif /* __SEAHORSE_DELETE_DIALOG_H__ */
