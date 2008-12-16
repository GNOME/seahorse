/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "config.h"

#include "seahorse-object-widget.h"

#define PROPERTIES "object-properties"

enum {
	PROP_0,
	PROP_OBJECT
};

static void	seahorse_object_widget_class_init	(SeahorseObjectWidgetClass	*klass);
static void	seahorse_object_widget_finalize		(GObject		*gobject);
static void	seahorse_object_widget_set_property	(GObject		*object,
							 guint			prop_id,
							 const GValue		*value,
							 GParamSpec		*pspec);
static void	seahorse_object_widget_get_property	(GObject		*object,
							 guint			prop_id,
							 GValue			*value,
							 GParamSpec		*pspec);
/* signal functions */
static void	seahorse_object_widget_destroyed	(gpointer data,
           	                                	 GObject *was);

static SeahorseWidgetClass	*parent_class		= NULL;

/* Hash set of widget hash sets with the object id as the object */
static GHashTable *types = NULL;

/* Hash set of window groups with the object id as the object */
static GHashTable *groups = NULL;

GType
seahorse_object_widget_get_type (void)
{
	static GType object_widget_type = 0;
	
	if (!object_widget_type) {
		static const GTypeInfo object_widget_info =
		{
			sizeof (SeahorseObjectWidgetClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_object_widget_class_init,
			NULL, NULL,
			sizeof (SeahorseObjectWidget),
			0, NULL
		};
		
		object_widget_type = g_type_register_static (SEAHORSE_TYPE_WIDGET,
			"SeahorseObjectWidget",  &object_widget_info, 0);
	}
	
	return object_widget_type;
}

static void
seahorse_object_widget_class_init (SeahorseObjectWidgetClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = seahorse_object_widget_finalize;
	gobject_class->set_property = seahorse_object_widget_set_property;
	gobject_class->get_property = seahorse_object_widget_get_property;
	
	g_object_class_install_property (gobject_class, PROP_OBJECT,
		g_param_spec_object ("object",  "Seahorse Object",
				     "Seahorse Object of this widget",
				     SEAHORSE_TYPE_OBJECT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
seahorse_object_widget_finalize (GObject *gobject)
{
    SeahorseWidget *swidget;
    SeahorseObjectWidget *self;
    GHashTable *widgets = NULL;
    GtkWindowGroup *group = NULL;
    GQuark id;
    GtkWidget *widget;
    
    self = SEAHORSE_OBJECT_WIDGET (gobject);
    swidget = SEAHORSE_WIDGET (self);
    
    g_return_if_fail (SEAHORSE_IS_OBJECT (self->object));
    id = seahorse_object_get_id (self->object);
    
    /* get widgets hash from types */
    widgets = g_hash_table_lookup (types, GUINT_TO_POINTER (id));
    /* if have a widgets hash, remove the widget */
    if (widgets != NULL) {
        g_hash_table_remove (widgets, swidget->name);
        /* if there are no more widgets, remove the hash */
        if (g_hash_table_size (widgets) == 0) {
            g_hash_table_remove (types, GUINT_TO_POINTER (id));
            /* if there are no more objects, destroy types */
            if (g_hash_table_size (types) == 0) {
                g_hash_table_destroy (types);
                types = NULL;
            }
        }
    }

    widget = seahorse_widget_get_toplevel (swidget);
    gtk_grab_remove (widget);

    /* Remove from parent */
    gtk_window_set_transient_for (GTK_WINDOW (widget), NULL);
    
    if (self->object)
	    g_object_weak_unref (G_OBJECT (self->object), seahorse_object_widget_destroyed, self); 
    self->object = NULL;

    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_object_widget_set_property (GObject *object, guint prop_id,
				  const GValue *value, GParamSpec *pspec)
{
	SeahorseObjectWidget *self;
	SeahorseWidget *swidget;
	
	self = SEAHORSE_OBJECT_WIDGET (object);
	swidget = SEAHORSE_WIDGET (object);
	
	switch (prop_id) {
		/* Watches object for destroy */
		case PROP_OBJECT:
			if (self->object)
				g_object_weak_unref (G_OBJECT (self->object), seahorse_object_widget_destroyed, self);
			self->object = g_value_get_object (value);
			if (self->object)
				g_object_weak_ref (G_OBJECT (self->object), seahorse_object_widget_destroyed, self);
			break;
		default:
			break;
	}
}

static void
seahorse_object_widget_get_property (GObject *object, guint prop_id,
				  GValue *value, GParamSpec *pspec)
{
	SeahorseObjectWidget *self;
	
	self = SEAHORSE_OBJECT_WIDGET (object);
	
	switch (prop_id) {
		case PROP_OBJECT:
			g_value_set_object (value, self->object);
			break;
		default:
			break;
	}
}

/* Called when object is destroyed */
static void
seahorse_object_widget_destroyed (gpointer data, GObject *object)
{
	SeahorseObjectWidget *self =  SEAHORSE_OBJECT_WIDGET (data);
	self->object = NULL;
	seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}

/* Tries to create a new object widget given parameters.
 * Makes use of hash sets to control which widgets can be created and how
 * the focus is grabbed.
 */
static SeahorseWidget*
seahorse_object_widget_create (gchar *name, GtkWindow *parent, SeahorseObject *object)
{
    SeahorseWidget *swidget = NULL;     // widget to lookup or create
    GHashTable *widgets = NULL;         // hash of widgets from types
    GQuark id;                          // hash key
    GtkWindowGroup *group = NULL;       // window group from groups
    GtkWidget *widget;                  // main window widget of swidget
    
    id = seahorse_object_get_id (object);
    
    /* if don't have a types hash, create one */
    if (types == NULL)
        types = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL, (GDestroyNotify)g_hash_table_destroy);
    
    /* otherwise lookup the widgets hash for the object */
    else
        widgets = g_hash_table_lookup (types, GUINT_TO_POINTER (id));
    
    /* if don't have a widgets hash for a object, create one and insert it */
    if (widgets == NULL) {
        widgets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        g_hash_table_insert (types, GUINT_TO_POINTER (id), widgets);
        
    /* otherwise lookup the widget */
    } else {
        swidget = g_hash_table_lookup (widgets, name);
    }
    
    /* if already have a widget of that type for the object, return null */
    if (swidget != NULL) {
        gtk_window_present (GTK_WINDOW (seahorse_widget_get_toplevel (swidget)));
        return NULL;
    }
    
    /* otherwise create a new widget & insert into widgets hash for the object */
    swidget = g_object_new (SEAHORSE_TYPE_OBJECT_WIDGET, "name", name,
                            "object", object, NULL);
    g_hash_table_insert (widgets, g_strdup (name), swidget);
    
    /* if don't have a groups hash, create one */
    if (groups == NULL)
        groups = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL, g_object_unref);
    
    /* otherwise lookup the group for that object */
    else
        group = g_hash_table_lookup (groups, GUINT_TO_POINTER (id));
    
    /* if don't have a group for the object, create one and insert it */
    if (group == NULL) {
        group = gtk_window_group_new ();
        g_hash_table_insert (groups, GUINT_TO_POINTER (id), group);
    }
    
    /* get window, add it to the group, grab it, return it */
    widget = seahorse_widget_get_toplevel (swidget);
    gtk_window_group_add_window (group, GTK_WINDOW (widget));
    gtk_grab_add (widget);
    
    if (parent != NULL)
        gtk_window_set_transient_for (GTK_WINDOW (widget), parent);
    
    return swidget;
}

SeahorseWidget*
seahorse_object_widget_new (gchar *name, GtkWindow *parent, SeahorseObject *object)
{
    SeahorseWidget *swidget;
    
    swidget = seahorse_object_widget_create (name, parent, object);
    
    /* We don't care about this floating business */
    g_object_ref (GTK_OBJECT (swidget));
    gtk_object_sink (GTK_OBJECT (swidget));

    return swidget;
}
