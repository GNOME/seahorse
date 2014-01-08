/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include "config.h"

#include "seahorse-certificate.h"
#include "seahorse-pkcs11-deleter.h"
#include "seahorse-pkcs11-key-deleter.h"
#include "seahorse-pkcs11-properties.h"
#include "seahorse-pkcs11-request.h"
#include "seahorse-private-key.h"

#include "seahorse-common.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

#include <glib/gi18n.h>

static const gchar *UI_STRING =
"<ui>"
"	<toolbar name='Toolbar'>"
"		<toolitem action='export-object'/>"
"		<toolitem action='delete-object'/>"
"		<separator name='MiddleSeparator' expand='true'/>"
"		<toolitem action='request-certificate'/>"
"	</toolbar>"
"</ui>";

#define SEAHORSE_PKCS11_PROPERTIES_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_PROPERTIES, SeahorsePkcs11PropertiesClass))
#define SEAHORSE_IS_PKCS11_PROPERTIES_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_PROPERTIES))
#define SEAHORSE_PKCS11_PROPERTIES_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_PROPERTIES, SeahorsePkcs11PropertiesClass))

typedef struct _SeahorsePkcs11PropertiesClass SeahorsePkcs11PropertiesClass;

struct _SeahorsePkcs11Properties {
	GtkWindow parent;
	GtkBox *content;
	GcrViewer *viewer;
	GObject *object;
	GCancellable *cancellable;
	GckObject *request_key;
	GtkUIManager *ui_manager;
	GtkActionGroup *actions;
};

struct _SeahorsePkcs11PropertiesClass {
	GtkWindowClass parent_class;
};

enum {
	PROP_0,
	PROP_OBJECT
};

G_DEFINE_TYPE (SeahorsePkcs11Properties, seahorse_pkcs11_properties, GTK_TYPE_WINDOW);

static void
seahorse_pkcs11_properties_init (SeahorsePkcs11Properties *self)
{
	GtkWidget *viewer;

	self->cancellable = g_cancellable_new ();

	gtk_window_set_default_size (GTK_WINDOW (self), 400, 400);

	self->content = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
	gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->content));
	gtk_widget_show (GTK_WIDGET (self->content));

	self->viewer = gcr_viewer_new_scrolled ();
	viewer = GTK_WIDGET (self->viewer);
	gtk_container_add (GTK_CONTAINER (self->content), viewer);
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

			if (g_object_class_find_property (G_OBJECT_GET_CLASS (renderer), "object"))
				g_object_set (renderer, "object", object, NULL);

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
on_export_certificate (GtkAction *action,
                       gpointer user_data)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (user_data);
	GtkWindow *parent = GTK_WINDOW (self);
	GError *error = NULL;
	GList *objects;

	objects = g_list_append (NULL, self->object);
	seahorse_exportable_export_to_prompt_wait (objects, parent, &error);
	g_list_free (objects);

	if (error != NULL)
		seahorse_util_handle_error (&error, parent, _("Failed to export certificate"));
}

static void
on_delete_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (user_data);
	GError *error = NULL;

	if (seahorse_deleter_delete_finish (SEAHORSE_DELETER (source), result, &error))
		gtk_widget_destroy (GTK_WIDGET (self));

	else
		seahorse_util_handle_error (&error, self, _("Couldn't delete"));

	g_object_unref (self);
}

static void
on_delete_objects (GtkAction *action,
                   gpointer user_data)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (user_data);
	SeahorseDeleter *deleter;
	GObject *partner = NULL;

	g_object_get (self->object, "partner", &partner, NULL);

	if (partner || SEAHORSE_IS_PRIVATE_KEY (self->object)) {
		deleter = seahorse_pkcs11_key_deleter_new (self->object);
		if (!seahorse_deleter_add_object (SEAHORSE_DELETER (deleter), partner))
			g_assert_not_reached ();
	} else {
		deleter = seahorse_pkcs11_deleter_new (SEAHORSE_CERTIFICATE (self->object));
	}

	if (seahorse_deleter_prompt (deleter, GTK_WINDOW (self))) {
		seahorse_deleter_delete (deleter, NULL, on_delete_complete, g_object_ref (self));
	}

	g_object_unref (deleter);
	g_clear_object (&partner);
}

static void
on_request_certificate (GtkAction *action,
                        gpointer user_data)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (user_data);
	g_return_if_fail (self->request_key != NULL);
	seahorse_pkcs11_request_prompt (GTK_WINDOW (self), self->request_key);
}

static const GtkActionEntry UI_ACTIONS[] = {
	{ "export-object", GTK_STOCK_SAVE_AS, N_("_Export"), "",
	  N_("Export the certificate"), G_CALLBACK (on_export_certificate) },
	{ "delete-object", GTK_STOCK_DELETE, N_("_Delete"), "<Ctrl>Delete",
	  N_("Delete this certificate or key"), G_CALLBACK (on_delete_objects) },
	{ "request-certificate", NULL, N_("Request _Certificate"), NULL,
	  N_("Create a certificate request file for this key"), G_CALLBACK (on_request_certificate) },
};

static void
on_ui_manager_add_widget (GtkUIManager *manager,
                          GtkWidget *widget,
                          gpointer user_data)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (user_data);

	if (!GTK_IS_TOOLBAR (widget))
		return;

	gtk_box_pack_start (self->content, widget, FALSE, TRUE, 0);
	gtk_box_reorder_child (self->content, widget, 0);

	gtk_style_context_add_class (gtk_widget_get_style_context (widget),
	                             GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	gtk_widget_reset_style (widget);
	gtk_widget_show (widget);
}

typedef struct {
	SeahorsePkcs11Properties *properties;
	GckObject *private_key;
} CapableClosure;

static void
on_certificate_request_capable (GObject *source,
                                GAsyncResult *result,
                                gpointer user_data)
{
	CapableClosure *closure = user_data;
	SeahorsePkcs11Properties *self = closure->properties;
	GError *error = NULL;
	GtkAction *request;

	if (gcr_certificate_request_capable_finish (result, &error)) {
		request = gtk_action_group_get_action (self->actions, "request-certificate");
		gtk_action_set_visible (request, TRUE);
		g_clear_object (&self->request_key);
		self->request_key = g_object_ref (closure->private_key);

	} else if (error != NULL) {
		g_message ("couldn't check capabilities of private key: %s", error->message);
		g_clear_error (&error);
	}

	g_object_unref (closure->properties);
	g_object_unref (closure->private_key);
	g_slice_free (CapableClosure, closure);
}

static void
check_certificate_request_capable (SeahorsePkcs11Properties *self,
                                   GObject *object)
{
	CapableClosure *closure;

	if (!SEAHORSE_IS_PRIVATE_KEY (object))
		return;

	closure = g_slice_new (CapableClosure);
	closure->properties = g_object_ref (self);
	closure->private_key = g_object_ref (object);

	gcr_certificate_request_capable_async (closure->private_key,
	                                       self->cancellable,
	                                       on_certificate_request_capable,
	                                       closure);
}

static void
seahorse_pkcs11_properties_constructed (GObject *obj)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);
	GObject *partner = NULL;
	GError *error = NULL;
	GList *exporters = NULL;
	GtkAction *request;
	GtkAction *action;

	G_OBJECT_CLASS (seahorse_pkcs11_properties_parent_class)->constructed (obj);

	self->actions = gtk_action_group_new ("Pkcs11Actions");
	gtk_action_group_set_translation_domain (self->actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (self->actions, UI_ACTIONS, G_N_ELEMENTS (UI_ACTIONS), self);
	action = gtk_action_group_get_action (self->actions, "delete-object");
	g_object_bind_property (self->object, "deletable", action, "sensitive", G_BINDING_SYNC_CREATE);
	action = gtk_action_group_get_action (self->actions, "export-object");
	g_object_bind_property (self->object, "exportable", action, "sensitive", G_BINDING_SYNC_CREATE);
	request = gtk_action_group_get_action (self->actions, "request-certificate");
	gtk_action_set_is_important (request, TRUE);
	gtk_action_set_visible (request, FALSE);

	self->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (self->ui_manager, self->actions, 0);
	g_signal_connect (self->ui_manager, "add-widget", G_CALLBACK (on_ui_manager_add_widget), self);
	gtk_ui_manager_add_ui_from_string (self->ui_manager, UI_STRING, -1, &error);
	if (error != NULL) {
		g_warning ("couldn't load ui: %s", error->message);
		g_error_free (error);
	}
	gtk_ui_manager_ensure_update (self->ui_manager);

	g_return_if_fail (self->object != NULL);

	g_signal_connect_object (self->object, "notify::label", G_CALLBACK (on_object_label_changed),
	                         self, G_CONNECT_AFTER);
	on_object_label_changed (self->object, NULL, self);

	add_renderer_for_object (self, self->object);
	check_certificate_request_capable (self, self->object);

	g_object_get (self->object, "partner", &partner, NULL);
	if (partner != NULL) {
		add_renderer_for_object (self, partner);
		check_certificate_request_capable (self, partner);

		g_object_unref (partner);
	}

	if (SEAHORSE_IS_EXPORTABLE (self->object))
		exporters = seahorse_exportable_create_exporters (SEAHORSE_EXPORTABLE (self->object),
		                                                  SEAHORSE_EXPORTER_TYPE_ANY);

	request = gtk_action_group_get_action (self->actions, "export-object");
	gtk_action_set_visible (request, exporters != NULL);
	g_list_free_full (exporters, g_object_unref);

	gtk_widget_grab_focus (GTK_WIDGET (self->viewer));
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
seahorse_pkcs11_properties_dispose (GObject *obj)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);

	g_cancellable_cancel (self->cancellable);

	G_OBJECT_CLASS (seahorse_pkcs11_properties_parent_class)->dispose (obj);
}

static void
seahorse_pkcs11_properties_finalize (GObject *obj)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);

	g_object_unref (self->cancellable);
	g_clear_object (&self->request_key);
	g_object_unref (self->actions);
	g_object_unref (self->ui_manager);
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
	gobject_class->dispose = seahorse_pkcs11_properties_dispose;
	gobject_class->finalize = seahorse_pkcs11_properties_finalize;

	g_object_class_install_property (gobject_class, PROP_OBJECT,
	           g_param_spec_object ("object", "Object", "Certificate or key to display", G_TYPE_OBJECT,
	                                G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/* -----------------------------------------------------------------------------
 * PUBLIC
 */

GtkWindow *
seahorse_pkcs11_properties_new (GObject  *object,
                                GtkWindow *parent)
{
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
