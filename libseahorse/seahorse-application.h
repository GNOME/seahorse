/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
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

#ifndef __SEAHORSE_APPLICATION_H__
#define __SEAHORSE_APPLICATION_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#define SEAHORSE_TYPE_APPLICATION                   (seahorse_application_get_type ())
#define SEAHORSE_APPLICATION(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_APPLICATION, SeahorseApplication))
#define SEAHORSE_APPLICATION_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_APPLICATION, SeahorseApplicationClass))
#define SEAHORSE_IS_APPLICATION(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_APPLICATION))
#define SEAHORSE_IS_APPLICATION_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_APPLICATION))
#define SEAHORSE_APPLICATION_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_APPLICATION, SeahorseApplicationClass))

typedef struct _SeahorseApplication SeahorseApplication;
typedef struct _SeahorseApplicationClass SeahorseApplicationClass;

GType               seahorse_application_get_type                (void);

GtkApplication *    seahorse_application_new                     (void);

GtkApplication *    seahorse_application_get                     (void);

GSettings *         seahorse_application_settings                (SeahorseApplication *self);

GSettings *         seahorse_application_pgp_settings            (SeahorseApplication *self);

#endif /* __SEAHORSE_APPLICATION_H__ */
