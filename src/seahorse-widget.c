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
	PROP_CTX,
	PROP_COMPONENT
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
/* component signal functions */
static void		seahorse_widget_show_status	(const SeahorseContext	*sctx,
							 const gchar		*status,
							 SeahorseWidget		*swidget);
static void		seahorse_widget_show_bar	(GtkCheckMenuItem	*checkmenuitem,
							 GtkWidget		*bar);
static gboolean		seahorse_widget_focus_in_event	(GtkWidget		*widget,
							 GdkEventFocus		*event,
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
		g_param_spec_string ("name", _("Widget name"),
				     _("Name of glade file and main widget"),
				     NULL, G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, PROP_CTX,
		g_param_spec_object ("ctx", _("Seahorse Context"),
				     _("Current Seahorse Context to use"),
				     SEAHORSE_TYPE_CONTEXT, G_PARAM_READWRITE));
	g_object_class_install_property (gobject_class, PROP_COMPONENT,
		g_param_spec_boolean ("component", _("If Seahorse Component"),
				      _("Seahorse Components are GnomeApps with extra signals"),
				      FALSE, G_PARAM_READWRITE));
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
	
	g_signal_handlers_disconnect_by_func (swidget->sctx, seahorse_widget_show_status, swidget);
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
	
	swidget = SEAHORSE_WIDGET (object);
	
	switch (prop_id) {
		/* Loads xml definition from name, connects common callbacks */
		case PROP_NAME:
			swidget->name = g_value_dup_string (value);
			swidget->xml = glade_xml_new (g_strdup_printf ("%sseahorse-%s.glade2",
				SEAHORSE_GLADEDIR, swidget->name), swidget->name, NULL);
			
			glade_xml_signal_connect_data (swidget->xml, "closed",
				G_CALLBACK (seahorse_widget_closed), swidget);
			glade_xml_signal_connect_data (swidget->xml, "delete_event",
				G_CALLBACK (seahorse_widget_delete_event), swidget);
			glade_xml_signal_connect_data (swidget->xml, "help",
				G_CALLBACK (seahorse_widget_show_help), swidget);
			break;
		case PROP_CTX:
			swidget->sctx = g_value_get_object (value);
			g_object_ref (G_OBJECT (swidget->sctx));
			break;
		/* Connects component specific callbacks */
		case PROP_COMPONENT:
			if (g_value_get_boolean (value)) {
				glade_xml_signal_connect_data (swidget->xml, "toolbar_activate",
					G_CALLBACK (seahorse_widget_show_bar),
					glade_xml_get_widget (swidget->xml, "tool_dock"));
				glade_xml_signal_connect_data (swidget->xml, "statusbar_activate",
					G_CALLBACK (seahorse_widget_show_bar),
					glade_xml_get_widget (swidget->xml, STATUS));
				glade_xml_signal_connect_data (swidget->xml, "focus_in_event",
					G_CALLBACK (seahorse_widget_focus_in_event), swidget);
				g_signal_connect (swidget->sctx, STATUS,
					G_CALLBACK (seahorse_widget_show_status), swidget);
			}
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

/* Shows status in the window's status bar */
static void
seahorse_widget_show_status (const SeahorseContext *sctx, const gchar *status, SeahorseWidget *swidget)
{
	GnomeAppBar *appbar;
	
	appbar = GNOME_APPBAR (glade_xml_get_widget (swidget->xml, STATUS));
	gnome_appbar_set_status (appbar, status);
}

/* Toggles display of a window bar */
static void
seahorse_widget_show_bar (GtkCheckMenuItem *checkmenuitem, GtkWidget *bar)
{
	if (gtk_check_menu_item_get_active (checkmenuitem))
		gtk_widget_show (bar);
	else
		gtk_widget_hide (bar);
}

/* Gpgme callback to show an operation's progress */
static void
show_progress (gpointer widget, const gchar *what, gint type, gint current, gint total)
{
	SeahorseWidget *swidget;
	GnomeAppBar *status;
	GtkProgressBar *progress;
	gdouble fract;
	
	swidget = SEAHORSE_WIDGET (widget);
	status = GNOME_APPBAR (glade_xml_get_widget (swidget->xml, STATUS));
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, swidget->name), FALSE);
	
	gnome_appbar_set_status (status, what);
	progress = gnome_appbar_get_progress (status);
		
	if (!current) {
		gtk_progress_bar_set_pulse_step (progress, 0.05);
		gtk_progress_bar_pulse (progress);
	}
	else {
		fract = (gdouble) current / (gdouble) total;
		gtk_progress_bar_set_fraction (progress, fract);
	}
	
	while (g_main_context_pending (NULL))
		g_main_context_iteration (NULL, TRUE);
	
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, swidget->name), TRUE);
}

/* Changes progress callback to current focused widget */
static gboolean
seahorse_widget_focus_in_event (GtkWidget *widget, GdkEventFocus *event, SeahorseWidget *swidget)
{
	gpgme_set_progress_cb (swidget->sctx->ctx, show_progress, swidget);
	return TRUE;
}

/* Common function for creating new widget */
static SeahorseWidget*
seahorse_widget_create (gchar *name, SeahorseContext *sctx, gboolean component)
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
	swidget = g_object_new (SEAHORSE_TYPE_WIDGET, "name", name, "ctx", sctx, "component", component, NULL);
	g_hash_table_insert (widgets, g_strdup (name), swidget);
	
	return swidget;
}

SeahorseWidget*
seahorse_widget_new (gchar *name, SeahorseContext *sctx)
{
	return seahorse_widget_create (name, sctx, FALSE);
}

SeahorseWidget*
seahorse_widget_new_component (gchar *name, SeahorseContext *sctx)
{
	return seahorse_widget_create (name, sctx, TRUE);	
}

SeahorseWidget*
seahorse_widget_new_allow_multiple (gchar *name, SeahorseContext *sctx)
{
	return g_object_new (SEAHORSE_TYPE_WIDGET, "name", name, "ctx", sctx, NULL);
}

void
seahorse_widget_destroy (SeahorseWidget *swidget)
{
	g_return_if_fail (swidget != NULL && SEAHORSE_IS_WIDGET (swidget));
	
	g_object_unref (swidget);
}
