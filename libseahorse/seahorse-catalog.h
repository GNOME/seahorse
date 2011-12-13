/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 */

#ifndef __SEAHORSE_CATALOG_H__
#define __SEAHORSE_CATALOG_H__

#include <glib-object.h>

#include "seahorse-widget.h"

#define SEAHORSE_CATALOG_MENU_OBJECT   "ObjectPopup"

#define SEAHORSE_TYPE_CATALOG               (seahorse_catalog_get_type ())
#define SEAHORSE_CATALOG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_CATALOG, SeahorseCatalog))
#define SEAHORSE_CATALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_CATALOG, SeahorseCatalogClass))
#define SEAHORSE_IS_CATALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_CATALOG))
#define SEAHORSE_IS_CATALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_CATALOG))
#define SEAHORSE_CATALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_CATALOG, SeahorseCatalogClass))

typedef struct _SeahorseCatalog SeahorseCatalog;
typedef struct _SeahorseCatalogClass SeahorseCatalogClass;
typedef struct _SeahorseCatalogPrivate SeahorseCatalogPrivate;

struct _SeahorseCatalog {
	SeahorseWidget parent;
	SeahorseCatalogPrivate *pv;
};

struct _SeahorseCatalogClass {
	SeahorseWidgetClass parent;

	GList *          (*get_backends)                  (SeahorseCatalog *self);

	SeahorsePlace *  (*get_focused_place)             (SeahorseCatalog *self);

	GList *          (*get_selected_objects)          (SeahorseCatalog *self);

	void             (*selection_changed)             (SeahorseCatalog *catalog);
};

GType               seahorse_catalog_get_type                    (void);

void                seahorse_catalog_ensure_updated              (SeahorseCatalog* self);

void                seahorse_catalog_include_actions             (SeahorseCatalog* self,
                                                                  GtkActionGroup* actions);

GList *             seahorse_catalog_get_selected_objects        (SeahorseCatalog* self);

SeahorsePlace *     seahorse_catalog_get_focused_place           (SeahorseCatalog* self);

GList *             seahorse_catalog_get_backends                (SeahorseCatalog* self);

void                seahorse_catalog_show_context_menu           (SeahorseCatalog* self,
                                                                  const gchar *which,
                                                                  guint button,
                                                                  guint time);

void                seahorse_catalog_show_properties             (SeahorseCatalog* self,
                                                                  GObject* obj);

GtkWindow *         seahorse_catalog_get_window                  (SeahorseCatalog *self);

#endif /* __SEAHORSE_CATALOG_H__ */
