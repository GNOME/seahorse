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

/* Parts from evolution-data-server */

#ifndef __SEAHORSE_PROGRESS_H__
#define __SEAHORSE_PROGRESS_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

void          seahorse_progress_prep            (GCancellable *cancellable,
                                                 gconstpointer progress_tag,
                                                 const gchar *detail,
                                                 ...);

void          seahorse_progress_begin           (GCancellable *cancellable,
                                                 gconstpointer progress_tag);

void          seahorse_progress_prep_and_begin  (GCancellable *cancellable,
                                                 gconstpointer progress_tag,
                                                 const gchar *detail,
                                                 ...);

void          seahorse_progress_update          (GCancellable *cancellable,
                                                 gconstpointer progress_tag,
                                                 const gchar *detail,
                                                 ...);

void          seahorse_progress_end             (GCancellable *cancellable,
                                                 gconstpointer progress_tag);

void          seahorse_progress_show            (GCancellable *cancellable,
                                                 const gchar *title,
                                                 gboolean delayed);

void          seahorse_progress_show_with_notice (GCancellable *cancellable,
                                                 const gchar *title,
                                                 const gchar *notice,
                                                 gboolean delayed);

void          seahorse_progress_attach           (GCancellable *cancellable,
                                                  GtkBuilder *builder);

#endif /* __SEAHORSE_PROGRESS_H__ */
