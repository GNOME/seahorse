/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Stefan Walter
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
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SeahorseWidget: A window created from a glade file.
 * 
 * - All SeahorseWidget objects are destroyed when the SeahorseContext 
 *   goes bye-bye.
 * - Implements fun GtkUIManager stuff.
 * 
 * Signals:
 *   destroy: The window was destroyed.
 *
 * Properties: 
 *   name: (gchar*) The name of the glade file to load. 
 */
 
#ifndef __SEAHORSE_WIDGET_H__
#define __SEAHORSE_WIDGET_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>

#define SEAHORSE_TYPE_WIDGET            (seahorse_widget_get_type ())
#define SEAHORSE_WIDGET(obj)            (GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_WIDGET, SeahorseWidget))
#define SEAHORSE_WIDGET_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_WIDGET, SeahorseWidgetClass))
#define SEAHORSE_IS_WIDGET(obj)         (GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_WIDGET))
#define SEAHORSE_IS_WIDGET_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_WIDGET))
#define SEAHORSE_WIDGET_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_WIDGET, SeahorseWidgetClass))

typedef struct _SeahorseWidget SeahorseWidget;
typedef struct _SeahorseWidgetClass SeahorseWidgetClass;

struct _SeahorseWidget {
    GtkObject parent;

    /*< public >*/
    GladeXML *xml;
    gchar *name;
    
    /*< private >*/
    GtkUIManager   *ui;
    gboolean destroying;
};

struct _SeahorseWidgetClass {
    GtkObjectClass parent_class;
};

GType            seahorse_widget_get_type ();

SeahorseWidget*  seahorse_widget_new                (const gchar      *name,
                                                     GtkWindow        *parent);

SeahorseWidget*  seahorse_widget_new_allow_multiple (const gchar      *name,
                                                     GtkWindow        *parent);

SeahorseWidget*  seahorse_widget_find               (const gchar      *name);

GtkWidget*       seahorse_widget_get_top            (SeahorseWidget   *swidget);

GtkWidget*       seahorse_widget_get_widget         (SeahorseWidget   *swidget,
                                                     const char       *identifier);

void             seahorse_widget_show               (SeahorseWidget   *swidget);

void             seahorse_widget_show_help          (SeahorseWidget   *swidget);

void             seahorse_widget_set_visible        (SeahorseWidget   *swidget,
                                                     const char       *identifier,
                                                     gboolean         visible);

void             seahorse_widget_set_sensitive      (SeahorseWidget   *swidget,
                                                     const char       *identifier,
                                                     gboolean         sensitive);

void             seahorse_widget_destroy            (SeahorseWidget   *swidget);

GtkWidget*       seahorse_widget_get_ui_widget      (SeahorseWidget *swidget, 
                                                     const gchar *path);

void             seahorse_widget_add_actions        (SeahorseWidget   *swidget,
                                                     GtkActionGroup   *actions);

GtkActionGroup*  seahorse_widget_find_actions       (SeahorseWidget   *swidget, 
                                                     const gchar *name);

#endif /* __SEAHORSE_WIDGET_H__ */
