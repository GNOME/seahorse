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

#include "config.h"

#include "seahorse-certificate.h"
#include "seahorse-pkcs11-properties.h"

#include <gcr/gcr.h>

#define SEAHORSE_PKCS11_PROPERTIES_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_PROPERTIES, SeahorsePkcs11PropertiesClass))
#define SEAHORSE_IS_PKCS11_PROPERTIES_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_PROPERTIES))
#define SEAHORSE_PKCS11_PROPERTIES_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_PROPERTIES, SeahorsePkcs11PropertiesClass))

typedef struct _SeahorsePkcs11PropertiesClass SeahorsePkcs11PropertiesClass;

struct _SeahorsePkcs11Properties {
	GtkWindow parent;
	GcrViewer *viewer;
	GObject *object;
};

struct _SeahorsePkcs11PropertiesClass {
	GtkWindowClass parent_class;
};

enum {
	PROP_0,
	PROP_OBJECT
};

static GQuark QUARK_WINDOW = 0;

G_DEFINE_TYPE (SeahorsePkcs11Properties, seahorse_pkcs11_properties, GTK_TYPE_WINDOW);

static void
seahorse_pkcs11_properties_init (SeahorsePkcs11Properties *self)
{
	GtkWidget *viewer;

	gtk_window_set_default_size (GTK_WINDOW (self), 400, 400);

	self->viewer = gcr_viewer_new_scrolled ();
	viewer = GTK_WIDGET (self->viewer);
	gtk_container_add (GTK_CONTAINER (self), viewer);
	gtk_widget_set_hexpand (viewer, TRUE);
	gtk_widget_set_vexpand (viewer, TRUE);
	gtk_widget_show (viewer);
}

static void
add_renderer_for_object (SeahorsePkcs11Properties *self,
                         GObject *object)
{
	GckAttributes *attributes = NULL;
	GcrRenderer *renderer;
	gchar *label = NULL;

	g_object_get (object, "label", &label, "attributes", &attributes, NULL);
	if (attributes != NULL) {
		renderer = gcr_renderer_create (label, attributes);
		if (renderer) {
			g_object_bind_property (object, "label",
			                        renderer, "label",
			                        G_BINDING_DEFAULT);
			g_object_bind_property (object, "attributes",
			                        renderer, "attributes",
			                        G_BINDING_DEFAULT);

			gcr_viewer_add_renderer (self->viewer, renderer);
			g_object_unref (renderer);
		}
		gck_attributes_unref (attributes);
	}

	g_free (label);
}

static void
on_object_label_changed (GObject *obj,
                         GParamSpec *spec,
                         gpointer user_data)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (user_data);
	gchar *description = NULL;
	gchar *label = NULL;
	gchar *title;

	g_object_get (obj, "label", &label, "description", &description, NULL);
	if (label && !label[0]) {
		g_free (label);
		label = NULL;
		label = _("Unnamed");
	}

	title = g_strdup_printf ("%s - %s",
	                         label ? label : _("Unnamed"),
	                         description);

	g_free (label);
	g_free (description);

	gtk_window_set_title (GTK_WINDOW (self), title);
	g_free (title);
}

static void
seahorse_pkcs11_properties_constructed (GObject *obj)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);
	GObject *partner = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_properties_parent_class)->constructed (obj);

	g_return_if_fail (self->object != NULL);
	g_object_set_qdata (self->object, QUARK_WINDOW, self);

	g_signal_connect (self->object, "notify::label", G_CALLBACK (on_object_label_changed), self);
	on_object_label_changed (self->object, NULL, self);

	add_renderer_for_object (self, self->object);

	g_object_get (self->object, "partner", &partner, NULL);
	if (partner != NULL) {
		add_renderer_for_object (self, partner);
		g_object_unref (partner);
	}
}

static void
seahorse_pkcs11_properties_set_property (GObject *obj,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);

	switch (prop_id) {
	case PROP_OBJECT:
		g_return_if_fail (self->object == NULL);
		self->object = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_properties_get_property (GObject *obj,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);

	switch (prop_id) {
	case PROP_OBJECT:
		g_value_set_object (value, seahorse_pkcs11_properties_get_object (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_properties_finalize (GObject *obj)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);

	if (self->object) {
		if (g_object_get_qdata (self->object, QUARK_WINDOW) == self)
			g_object_set_qdata (self->object, QUARK_WINDOW, NULL);
	}

	g_clear_object (&self->object);

	G_OBJECT_CLASS (seahorse_pkcs11_properties_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_properties_class_init (SeahorsePkcs11PropertiesClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	seahorse_pkcs11_properties_parent_class = g_type_class_peek_parent (klass);

	gobject_class->constructed = seahorse_pkcs11_properties_constructed;
	gobject_class->set_property = seahorse_pkcs11_properties_set_property;
	gobject_class->get_property = seahorse_pkcs11_properties_get_property;
	gobject_class->finalize = seahorse_pkcs11_properties_finalize;

	g_object_class_install_property (gobject_class, PROP_OBJECT,
	           g_param_spec_object ("object", "Object", "Certificate or key to display", G_TYPE_OBJECT,
	                                G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	QUARK_WINDOW = g_quark_from_static_string ("seahorse-pkcs11-properties-window");
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

GtkWindow *
seahorse_pkcs11_properties_show (GObject  *object,
                                 GtkWindow *parent)
{
	GObject *previous;
	GtkWindow *window;

	/* Try to show an already present window */
	previous = g_object_get_qdata (object, QUARK_WINDOW);
	if (GTK_IS_WINDOW (previous)) {
		window = GTK_WINDOW (previous);
		if (gtk_widget_get_visible (GTK_WIDGET (window))) {
			gtk_window_present (window);
			return window;
		}
	}

	return g_object_new (SEAHORSE_TYPE_PKCS11_PROPERTIES,
	                     "object", object,
	                     "transient-for", parent,
	                     NULL);
}

GObject *
seahorse_pkcs11_properties_get_object (SeahorsePkcs11Properties *self)
{
	g_return_val_if_fail (SEAHORSE_IS_PKCS11_PROPERTIES (self), NULL);
	return self->object;
}
