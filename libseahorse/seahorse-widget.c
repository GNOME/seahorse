/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#include <gnome.h>
#include <gpgme.h>

#include "seahorse-widget.h"
#include "config.h"

#define STATUS "status"

enum {
	PROP_0,
	PROP_NAME,
	PROP_CTX
};

static void		seahorse_widget_class_init	(SeahorseWidgetClass	*klass);
static void		seahorse_widget_finalize	(GObject		*gobject);
static void		seahorse_widget_set_property	(GObject		*object,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void		seahorse_widget_get_property	(GObject		*object,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);
/* signal functions */
static void		seahorse_widget_show_help	(GtkWidget		*widget,
							 SeahorseWidget		*swidget);
static void		seahorse_widget_closed		(GtkWidget		*widget,
							 SeahorseWidget		*swidget);
static gboolean		seahorse_widget_delete_event	(GtkWidget		*widget,
							 GdkEvent		*event,
							 SeahorseWidget		*swidget);
static void		seahorse_widget_destroyed	(GtkObject		*object,
							 SeahorseWidget		*swidget);

static void		seahorse_widget_show_progress	(SeahorseContext	*sctx,
							 const gchar		*op,
							 gdouble		fract,
							 SeahorseWidget		*swidget);

static GObjectClass	*parent_class			= NULL;

/* Hash of widgets with name as key */
static GHashTable	*widgets			= NULL;

GType
seahorse_widget_get_type (void)
{
	static GType widget_type = 0;
	
	if (!widget_type) {
		static const GTypeInfo widget_info =
		{
			sizeof (SeahorseWidgetClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_widget_class_init,
			NULL, NULL,
			sizeof (SeahorseWidget),
			0, NULL
		};
		
		widget_type = g_type_register_static (G_TYPE_OBJECT, "SeahorseWidget", &widget_info, 0);
	}
	
	return widget_type;
}

static void
seahorse_widget_class_init (SeahorseWidgetClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_widget_finalize;
	gobject_class->set_property = seahorse_widget_set_property;
	gobject_class->get_property = seahorse_widget_get_property;
	
	g_object_class_install_property (gobject_class, PROP_NAME,
		g_param_spec_string ("name", "Widget name",
				     "Name of glade file and main widget",
				     NULL, G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, PROP_CTX,
		g_param_spec_object ("ctx", "Seahorse Context",
				     "Current Seahorse Context to use",
				     SEAHORSE_TYPE_CONTEXT, G_PARAM_READWRITE));
}

/* Disconnects callbacks, destroys main window widget,
 * and frees the xml definition and any other data */
static void
seahorse_widget_finalize (GObject *gobject)
{
	SeahorseWidget *swidget;
	
	swidget = SEAHORSE_WIDGET (gobject);
	
	/* Remove widget from hash and destroy hash if empty */
	g_hash_table_remove (widgets, swidget->name);
	if (g_hash_table_size == 0) {
		g_hash_table_destroy (widgets);
		widgets = NULL;
	}
	
	g_signal_handlers_disconnect_by_func (swidget->sctx, seahorse_widget_destroyed, swidget);
	g_signal_handlers_disconnect_by_func (swidget->sctx, seahorse_widget_show_progress, swidget);
	gtk_widget_destroy (glade_xml_get_widget (swidget->xml, swidget->name));
	
	g_free (swidget->xml);
	swidget->xml = NULL;
	
	g_object_unref (swidget->sctx);
	
	g_free (swidget->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SeahorseWidget *swidget;
	char *path;
	
	swidget = SEAHORSE_WIDGET (object);
	
	switch (prop_id) {
		/* Loads xml definition from name, connects common callbacks */
		case PROP_NAME:
			swidget->name = g_value_dup_string (value);
			path = g_strdup_printf ("%sseahorse-%s.glade",
					SEAHORSE_GLADEDIR, swidget->name);
			swidget->xml = glade_xml_new (path, swidget->name, NULL);
			g_free (path);
			g_assert (swidget->xml != NULL);
			
			glade_xml_signal_connect_data (swidget->xml, "closed",
				G_CALLBACK (seahorse_widget_closed), swidget);
			glade_xml_signal_connect_data (swidget->xml, "delete_event",
				G_CALLBACK (seahorse_widget_delete_event), swidget);
			glade_xml_signal_connect_data (swidget->xml, "help",
				G_CALLBACK (seahorse_widget_show_help), swidget);
		
			gtk_window_set_icon (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
				gdk_pixbuf_new_from_file (PIXMAPSDIR "seahorse.png", NULL));
			break;
		case PROP_CTX:
			swidget->sctx = g_value_get_object (value);
			g_object_ref (G_OBJECT (swidget->sctx));
			g_signal_connect_after (swidget->sctx, "destroy",
				G_CALLBACK (seahorse_widget_destroyed), swidget);
			//a bit dirty, but easy
			if (!g_str_equal (swidget->name, "key-manager"))
				g_signal_connect (swidget->sctx, "progress",
					G_CALLBACK (seahorse_widget_show_progress), swidget);
			break;
		default:
			break;
	}
}

static void
seahorse_widget_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SeahorseWidget *swidget;
	swidget = SEAHORSE_WIDGET (object);
	
	switch (prop_id) {
		case PROP_NAME:
			g_value_set_string (value, swidget->name);
			break;
		case PROP_CTX:
			g_value_set_object (value, swidget->sctx);
			break;
		default:
			break;
	}
}

/* Shows gnome help for widget */
static void
seahorse_widget_show_help (GtkWidget *widget, SeahorseWidget *swidget)
{
	//error check help
	if (g_str_equal (swidget->name, "key-manager"))
		gnome_help_display (PACKAGE, "toc", NULL);
	else
		gnome_help_display (PACKAGE, swidget->name, NULL);
}

/* Destroys widget */
static void
seahorse_widget_closed (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_widget_destroy (swidget);
}

/* Closed widget */
static gboolean
seahorse_widget_delete_event (GtkWidget *widget, GdkEvent *event, SeahorseWidget *swidget)
{
	seahorse_widget_closed (widget, swidget);
}

/* Destroy widget when context is destroyed */
static void
seahorse_widget_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
	seahorse_widget_destroy (swidget);
}

/* Shows operation progress if a component, otherwise just desensitizes window */
static void
seahorse_widget_show_progress (SeahorseContext *sctx, const gchar *op, gdouble fract, SeahorseWidget *swidget)
{
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, swidget->name), fract == -1);
}

/**
 * seahorse_widget_new:
 * @name: Name of widget, filename part of glade file, and name of main window
 * @sctx: #SeahorseContext
 *
 * Creates a new #SeahorseWidget.
 *
 * Returns: The new #SeahorseWidget, or NULL if the widget already exists
 **/
SeahorseWidget*
seahorse_widget_new (gchar *name, SeahorseContext *sctx)
{
	SeahorseWidget *swidget;
	
	g_return_val_if_fail (SEAHORSE_IS_CONTEXT (sctx), NULL);
	
	/* Check if have widget hash */
	if (widgets != NULL) {
		swidget = g_hash_table_lookup (widgets, name);
		
		/* If widget already exists, present */
		if (swidget != NULL) {
			gtk_window_present (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
			return NULL;
		}
	}
	/* Else create new widget hash */
	else
		widgets = g_hash_table_new ((GHashFunc)g_str_hash, (GCompareFunc)g_str_equal);
	
	/* If widget doesn't already exist, create & insert into hash */
	swidget = g_object_new (SEAHORSE_TYPE_WIDGET, "name", name, "ctx", sctx, NULL);
	g_hash_table_insert (widgets, g_strdup (name), swidget);
	
	return swidget;
}

/**
 * seahorse_widget_new:
 * @name: Name of widget, filename part of glade file, and name of main window
 * @sctx: #SeahorseContext
 *
 * Creates a new #SeahorseWidget without checking if it already exists.
 *
 * Returns: The new #SeahorseWidget
 **/
SeahorseWidget*
seahorse_widget_new_allow_multiple (gchar *name, SeahorseContext *sctx)
{
	return g_object_new (SEAHORSE_TYPE_WIDGET, "name", name, "ctx", sctx, NULL);
}

/**
 * seahorse_widget_destroy:
 * @swidget: #SeahorseWidget to destroy
 *
 * Unrefs @swidget.
 **/
void
seahorse_widget_destroy (SeahorseWidget *swidget)
{
	g_return_if_fail (swidget != NULL && SEAHORSE_IS_WIDGET (swidget));
	
	g_object_unref (swidget);
}
