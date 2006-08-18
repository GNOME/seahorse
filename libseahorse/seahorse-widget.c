/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
 * Copyright (C) 2005 Nate Nielsen
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
#include <glade/glade.h>
#include <glade/glade-build.h>

#include "seahorse-widget.h"
#include "config.h"
#include "seahorse-gtkstock.h"

#define STATUS "status"

enum {
	PROP_0,
	PROP_NAME
};

static void     class_init          (SeahorseWidgetClass    *klass);
static void     object_init         (SeahorseWidget         *swidget);

static void     object_finalize     (GObject                *gobject);

static void     object_set_property (GObject                *object,
                                     guint                  prop_id,
                                     const GValue           *value,
                                     GParamSpec             *pspec);

static void     object_get_property (GObject                *object,
                                     guint                  prop_id,
                                     GValue                 *value,
                                     GParamSpec             *pspec);

/* signal functions */
static void     widget_closed        (GtkWidget             *widget,
                                      SeahorseWidget        *swidget);

static void     widget_help          (GtkWidget             *widget, 
                                      SeahorseWidget        *swidget);

static gboolean widget_delete_event  (GtkWidget             *widget,
                                      GdkEvent              *event,
                                      SeahorseWidget        *swidget);

static void     context_destroyed    (GtkObject             *object,
                                      SeahorseWidget        *swidget);

static GtkObjectClass *parent_class = NULL;

/* Hash of widgets with name as key */
static GHashTable *widgets = NULL;

GType
seahorse_widget_get_type (void)
{
	static GType widget_type = 0;
	
	if (!widget_type) {
		static const GTypeInfo widget_info = {
			sizeof (SeahorseWidgetClass), NULL, NULL,
			(GClassInitFunc) class_init,
			NULL, NULL, sizeof (SeahorseWidget), 0, (GInstanceInitFunc) object_init
		};
		
        widget_type = g_type_register_static (GTK_TYPE_OBJECT, "SeahorseWidget", 
                                              &widget_info, 0);
	}
	
	return widget_type;
}

static void
class_init (SeahorseWidgetClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = object_finalize;
	gobject_class->set_property = object_set_property;
	gobject_class->get_property = object_get_property;
	
    g_object_class_install_property (gobject_class, PROP_NAME,
        g_param_spec_string ("name", "Widget name", "Name of glade file and main widget",
                             NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/* Destroy widget when context is destroyed */
static void
context_destroyed (GtkObject *object, SeahorseWidget *swidget)
{
	seahorse_widget_destroy (swidget);
}

static void
object_init (SeahorseWidget *swidget)
{
    g_signal_connect_after (SCTX_APP(), "destroy", 
                G_CALLBACK (context_destroyed), swidget);
}

/* Disconnects callbacks, destroys main window widget,
 * and frees the xml definition and any other data */
static void
object_finalize (GObject *gobject)
{
	SeahorseWidget *swidget;
	
	swidget = SEAHORSE_WIDGET (gobject);
	
	/* Remove widget from hash and destroy hash if empty */
    if (widgets) {
    	g_hash_table_remove (widgets, swidget->name);
    	if (g_hash_table_size == 0) {
    		g_hash_table_destroy (widgets);
    		widgets = NULL;
    	}
    }

	g_signal_handlers_disconnect_by_func (SCTX_APP (), context_destroyed, swidget);
    if (glade_xml_get_widget (swidget->xml, swidget->name))
	    gtk_widget_destroy (glade_xml_get_widget (swidget->xml, swidget->name));
	
	g_object_unref (swidget->xml);
	swidget->xml = NULL;
	
	g_free (swidget->name);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
object_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    SeahorseWidget *swidget;
    GtkWidget *w;
    GdkPixbuf *pixbuf = NULL;
    char *path;
    
    swidget = SEAHORSE_WIDGET (object);
    
    switch (prop_id) {
    /* Loads xml definition from name, connects common callbacks */
    case PROP_NAME:
        g_return_if_fail (swidget->name == NULL);
        swidget->name = g_value_dup_string (value);
        path = g_strdup_printf ("%sseahorse-%s.glade",
                                SEAHORSE_GLADEDIR, swidget->name);
        swidget->xml = glade_xml_new (path, swidget->name, NULL);
        g_free (path);
        g_return_if_fail (swidget->xml != NULL);
        
        glade_xml_signal_connect_data (swidget->xml, "closed",
                                       G_CALLBACK (widget_closed), swidget);
        glade_xml_signal_connect_data (swidget->xml, "delete_event",
                                       G_CALLBACK (widget_delete_event), swidget);
        glade_xml_signal_connect_data (swidget->xml, "help",
                                       G_CALLBACK (widget_help), swidget);
        
        w = glade_xml_get_widget (swidget->xml, swidget->name);
        glade_xml_set_toplevel (swidget->xml, GTK_WINDOW (w));
        glade_xml_ensure_accel (swidget->xml);
        
        pixbuf = gtk_widget_render_icon (w, SEAHORSE_STOCK_SEAHORSE, 
                                         (GtkIconSize)-1, NULL); 
        gtk_window_set_icon (GTK_WINDOW (w), gdk_pixbuf_copy(pixbuf));
        g_object_unref(pixbuf);
        break;
    }
}

static void
object_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SeahorseWidget *swidget;
	swidget = SEAHORSE_WIDGET (object);
	
	switch (prop_id) {
		case PROP_NAME:
			g_value_set_string (value, swidget->name);
			break;
		default:
			break;
	}
}

static void 
widget_help (GtkWidget *widget, SeahorseWidget *swidget)
{
    seahorse_widget_show_help (swidget);
}

/* Destroys widget */
static void
widget_closed (GtkWidget *widget, SeahorseWidget *swidget)
{
	seahorse_widget_destroy (swidget);
}

/* Closed widget */
static gboolean
widget_delete_event (GtkWidget *widget, GdkEvent *event, SeahorseWidget *swidget)
{
	widget_closed (widget, swidget);
    return FALSE; /* propogate event */
}

/**
 * seahorse_widget_new:
 * @name: Name of widget, filename part of glade file, and name of main window
 *
 * Creates a new #SeahorseWidget.
 *
 * Returns: The new #SeahorseWidget, or NULL if the widget already exists
 **/
SeahorseWidget*
seahorse_widget_new (const gchar *name)
{
    /* Check if have widget hash */
    SeahorseWidget *swidget = seahorse_widget_find (name);
    
    /* If widget already exists, present */
    if (swidget != NULL) {
        gtk_window_present (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)));
        return NULL;
    }

    /* If widget doesn't already exist, create & insert into hash */
    swidget = g_object_new (SEAHORSE_TYPE_WIDGET, "name", name, NULL);
    if(!widgets)
        widgets = g_hash_table_new ((GHashFunc)g_str_hash, (GCompareFunc)g_str_equal);
    g_hash_table_insert (widgets, g_strdup (name), swidget);

    /* We don't care about this floating business */
    g_object_ref (GTK_OBJECT (swidget));
    gtk_object_sink (GTK_OBJECT (swidget));

    return swidget;
}

/**
 * seahorse_widget_new:
 * @name: Name of widget, filename part of glade file, and name of main window
 *
 * Creates a new #SeahorseWidget without checking if it already exists.
 *
 * Returns: The new #SeahorseWidget
 **/
SeahorseWidget*
seahorse_widget_new_allow_multiple (const gchar *name)
{
    SeahorseWidget *swidget = g_object_new (SEAHORSE_TYPE_WIDGET, "name", name,  NULL);
    
    /* We don't care about this floating business */
    g_object_ref (GTK_OBJECT (swidget));
    gtk_object_sink (GTK_OBJECT (swidget));

    return swidget;
}

SeahorseWidget*
seahorse_widget_find (const gchar *name)
{
    /* Check if have widget hash */
    if (widgets != NULL)
        return SEAHORSE_WIDGET (g_hash_table_lookup (widgets, name));
    return NULL;
}

/**
 * seahorse_widget_show_help
 * @swidget: The #SeahorseWidget.
 * 
 * Show help appropriate for the top level widget.
 */
void
seahorse_widget_show_help (SeahorseWidget *swidget)
{
    GError *err = NULL;

    if (g_str_equal (swidget->name, "key-manager"))
        gnome_help_display (PACKAGE, "introduction", &err);
    else
        gnome_help_display (PACKAGE, swidget->name, &err);

    if (err != NULL) {
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, 
                                         GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                                         _("Could not display help: %s"),
                                         err->message);
        g_signal_connect (G_OBJECT (dialog), "response",
                          G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_widget_show (dialog);
        g_error_free (err);
    }
}

/**
 * seahorse_widget_get_top
 * @swidget: The seahorse widget
 * 
 * Return the top level widget in this seahorse widget
 *
 * Returns: The top level widget
 **/
GtkWidget*      
seahorse_widget_get_top     (SeahorseWidget     *swidget)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_return_val_if_fail (widget != NULL, NULL);
    return widget;
}

GtkWidget*
seahorse_widget_get_widget (SeahorseWidget *swidget, const char *identifier)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, identifier);
    g_return_val_if_fail (widget != NULL, NULL);
    return widget;
}

/**
 * seahorse_widget_show:
 * @swidget: #SeahorseWidget to show
 * 
 * Show the toplevel widget in the glade file.
 **/
void
seahorse_widget_show (SeahorseWidget *swidget)
{
    GtkWidget *widget;

    if (swidget->ui)
        gtk_ui_manager_ensure_update (swidget->ui);

    widget = glade_xml_get_widget (swidget->xml, swidget->name);
    g_return_if_fail (widget != NULL);
    gtk_widget_show (widget);
}
 
void             
seahorse_widget_set_visible (SeahorseWidget *swidget, const char *identifier,
                             gboolean visible)
{
    GtkWidget *widget = glade_xml_get_widget (swidget->xml, identifier);
    g_return_if_fail (widget != NULL);
    
    if (visible)
        gtk_widget_show (widget);
    else
        gtk_widget_hide (widget);
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

/* UI MANAGER CODE ---------------------------------------------------------- */

static void
ui_add_widget (GtkUIManager *ui, GtkWidget *widget, SeahorseWidget *swidget)
{
    GtkWidget *holder;
    const gchar *name;
    
    /* We automatically add menus and toolbars */
    if (GTK_IS_MENU_BAR (widget))
        name = "menu-placeholder";
    else
        name = "toolbar-placeholder";
    
    if (name != NULL) {
        /* Find the appropriate position in the glade file */
        holder = glade_xml_get_widget (swidget->xml, name);
        if (holder != NULL)
            gtk_container_add (GTK_CONTAINER (holder), widget);
        else
            g_warning ("no place holder found for: %s", name);
    }
}

static void
ui_load (SeahorseWidget *swidget)
{
    GtkWidget *w;
    GError *err = NULL;
    gchar *path;
    
    if (!swidget->ui) {
        
        /* Load the menu/toolbar description file */
        swidget->ui = gtk_ui_manager_new ();
    	path = g_strdup_printf ("%sseahorse-%s.ui", SEAHORSE_GLADEDIR, swidget->name);
        gtk_ui_manager_add_ui_from_file (swidget->ui, path, &err);
		g_free (path);
        
        if (err) {
            g_warning ("couldn't load ui description for '%s': %s", swidget->name, err->message);
            g_error_free (err);
            return;
        }

        /* The widgets get added in an idle loop later */
        g_signal_connect (swidget->ui, "add-widget", G_CALLBACK (ui_add_widget), swidget);
        
        /* Attach accelerators to the window */
        w = glade_xml_get_widget (swidget->xml, swidget->name);
        if (GTK_IS_WINDOW (w))
            gtk_window_add_accel_group (GTK_WINDOW (w), gtk_ui_manager_get_accel_group (swidget->ui));
    }    
}

static void
cleanup_actions (GtkActionGroup *group)
{
    GList *actions, *l;
    
    #define ELIPSIS "..."
    #define ELIPSIS_LEN 3
    
    actions = gtk_action_group_list_actions (group);    
    
    for (l = actions; l; l = g_list_next (l)) {
        GtkAction *action = GTK_ACTION (l->data);
        gchar *label;
        guint len;
        
        /* Remove the ellipsis from the end of action labels if present */
        g_object_get (action, "short-label", &label, NULL);
        if (label) {
            len = strlen (label);
            if (strcmp (ELIPSIS, label + (len - ELIPSIS_LEN)) == 0) {
                label[len - ELIPSIS_LEN] = 0;
                g_object_set (action, "short-label", label, NULL);
            }
            g_free (label);
        }
    }
 
    g_list_free (actions);    
}

/**
 * seahorse_widget_get_ui_widget
 * @swidget: The #SeahorseWidget.
 * @path: The path to the widget. See gtk_ui_manager_get_widget
 * 
 * Returns a piece of generated UI. Note this doesn't look in the glade
 * file but rather looks in the GtkUIManager UI. If no UI has been loaded 
 * then one will be loaded. The UI file has the same name as the glade file 
 * but with a 'ui' extension. 
 */
GtkWidget*
seahorse_widget_get_ui_widget (SeahorseWidget *swidget, const gchar *path)
{
    g_return_val_if_fail (SEAHORSE_IS_WIDGET (swidget), NULL);
    
    ui_load (swidget);    
    g_return_val_if_fail (swidget->ui, NULL);
    
    return gtk_ui_manager_get_widget (swidget->ui, path);
}

/**
 * seahorse_widget_add_actions
 * @swidget: The #SeahorseWidget.
 * @actions: A #GtkActionGroup to add to the UI.
 * 
 * Adds a GtkActionGroup to this widget's GtkUIManager UI. If no UI
 * has been loaded then one will be loaded. The UI file has the same
 * name as the glade file but with a 'ui' extension. 
 */
void             
seahorse_widget_add_actions (SeahorseWidget *swidget, GtkActionGroup *actions)
{
    g_return_if_fail (SEAHORSE_IS_WIDGET (swidget));
    
    ui_load (swidget);    
    g_return_if_fail (swidget->ui);

    cleanup_actions (actions);
    gtk_ui_manager_insert_action_group (swidget->ui, actions, -1);
}

/** 
 * seahorse_widget_find_actions
 * @swidget: The #SeahorseWidget.
 * @name: The name of the action group.
 * 
 * Find an #GtkActionGroup previously added to this widget.
 * 
 * Returns: The action group.
 */
GtkActionGroup*
seahorse_widget_find_actions (SeahorseWidget *swidget, const gchar *name)
{
    GList *l;
    
    g_return_val_if_fail (SEAHORSE_IS_WIDGET (swidget), NULL);
    
    if (!swidget->ui)
        return NULL;
    
    for (l = gtk_ui_manager_get_action_groups (swidget->ui); l; l = g_list_next (l)) {
        if (g_str_equal (gtk_action_group_get_name (GTK_ACTION_GROUP (l->data)), name)) 
            return GTK_ACTION_GROUP (l->data);
    }
    
    return NULL;
}
