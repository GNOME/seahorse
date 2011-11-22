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
#include "seahorse-pkcs11-operations.h"
#include "seahorse-private-key.h"

#include "seahorse-delete-dialog.h"
#include "seahorse-progress.h"
#include "seahorse-util.h"

#include <gcr/gcr.h>

static const gchar *UI_STRING =
"<ui>"
"	<toolbar name='Toolbar'>"
"		<toolitem action='export-object'/>"
"		<toolitem action='delete-object'/>"
"		<separator name='MiddleSeparator' expand='true'/>"
"		<toolitem action='extra-action'/>"
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

static GQuark QUARK_WINDOW = 0;

G_DEFINE_TYPE (SeahorsePkcs11Properties, seahorse_pkcs11_properties, GTK_TYPE_WINDOW);

static void
seahorse_pkcs11_properties_init (SeahorsePkcs11Properties *self)
{
	GtkWidget *viewer;

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
	/* TODO: Exporter not done */
}

static void
on_delete_complete (GObject *source,
                    GAsyncResult *result,
                    gpointer user_data)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (user_data);
	GError *error = NULL;

	if (seahorse_pkcs11_delete_finish (result, &error))
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
	gboolean with_partner = FALSE;
	GObject *partner = NULL;
	GtkDialog *dialog = NULL;
	GCancellable *cancellable;
	GList *objects = NULL;
	gchar *label;

	g_object_get (self->object,
	              "label", &label,
	              "partner", &partner,
	              NULL);

	if (SEAHORSE_IS_CERTIFICATE (self->object)) {
		objects = g_list_prepend (objects, g_object_ref (self->object));
		dialog = seahorse_delete_dialog_new (GTK_WINDOW (self),
		                                     _("Are you sure you want to delete the certificate '%s'?"),
		                                     label);

		if (SEAHORSE_IS_PRIVATE_KEY (partner)) {
			seahorse_delete_dialog_set_check_value (SEAHORSE_DELETE_DIALOG (dialog), FALSE);
			seahorse_delete_dialog_set_check_label (SEAHORSE_DELETE_DIALOG (dialog),
			                                        _("Also delete matching private key"));
			with_partner = TRUE;
		}

	} else if (SEAHORSE_IS_PRIVATE_KEY (self->object)) {
		objects = g_list_prepend (objects, g_object_ref (self->object));
		dialog = seahorse_delete_dialog_new (GTK_WINDOW (self),
		                                     _("Are you sure you want to delete the private key '%s'?"),
		                                     label);

		seahorse_delete_dialog_set_check_value (SEAHORSE_DELETE_DIALOG (dialog), FALSE);
		seahorse_delete_dialog_set_check_require (SEAHORSE_DELETE_DIALOG (dialog), TRUE);
		seahorse_delete_dialog_set_check_label (SEAHORSE_DELETE_DIALOG (dialog),
		                                        _("I understand that this action cannot be undone"));
	}

	if (dialog && gtk_dialog_run (dialog) == GTK_RESPONSE_OK) {
		if (with_partner)
			objects = g_list_prepend (objects, partner);

		cancellable = g_cancellable_new ();
		seahorse_pkcs11_delete_async (objects, cancellable,
		                              on_delete_complete, g_object_ref (self));
		seahorse_progress_show (cancellable, _("Deleting"), TRUE);
		g_object_unref (cancellable);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_list_free_full (objects, g_object_unref);
	g_clear_object (&partner);
	g_free (label);
}

static void
on_extra_action (GtkAction *action,
                 gpointer user_data)
{

}


static const GtkActionEntry UI_ACTIONS[] = {
	{ "export-object", GTK_STOCK_SAVE_AS, N_("_Export"), "",
	  N_("Export the certificate"), G_CALLBACK (on_export_certificate) },
	{ "delete-object", GTK_STOCK_DELETE, N_("_Delete"), "<Ctrl>Delete",
	  N_("Delete this certificate or key"), G_CALLBACK (on_delete_objects) },
	{ "extra-action", NULL, N_("Extra _Action"), NULL,
	  N_("An extra action on the certificate"), G_CALLBACK (on_extra_action) },
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

static void
seahorse_pkcs11_properties_constructed (GObject *obj)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);
	GObject *partner = NULL;
	GError *error = NULL;

	G_OBJECT_CLASS (seahorse_pkcs11_properties_parent_class)->constructed (obj);

	self->actions = gtk_action_group_new ("Pkcs11Actions");
	gtk_action_group_add_actions (self->actions, UI_ACTIONS, G_N_ELEMENTS (UI_ACTIONS), self);

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
	g_object_set_qdata (self->object, QUARK_WINDOW, self);

	g_signal_connect_object (self->object, "notify::label", G_CALLBACK (on_object_label_changed),
	                         self, G_CONNECT_AFTER);
	on_object_label_changed (self->object, NULL, self);

	add_renderer_for_object (self, self->object);

	g_object_get (self->object, "partner", &partner, NULL);
	if (partner != NULL) {
		add_renderer_for_object (self, partner);
		g_object_unref (partner);
	}

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
seahorse_pkcs11_properties_finalize (GObject *obj)
{
	SeahorsePkcs11Properties *self = SEAHORSE_PKCS11_PROPERTIES (obj);

	g_object_unref (self->actions);
	g_object_unref (self->ui_manager);

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
