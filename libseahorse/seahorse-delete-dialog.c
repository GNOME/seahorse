/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "seahorse-delete-dialog.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define SEAHORSE_DELETE_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_DELETE_DIALOG, SeahorseDeleteDialogClass))
#define SEAHORSE_IS_DELETE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_DELETE_DIALOG))
#define SEAHORSE_DELETE_DIALOG_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_DELETE_DIALOG, SeahorseDeleteDialogClass))

typedef struct _SeahorseDeleteDialogClass SeahorseDeleteDialogClass;

struct _SeahorseDeleteDialog {
	GtkMessageDialog dialog;
	GtkWidget *check;
	gboolean check_require;
};

struct _SeahorseDeleteDialogClass {
	GtkMessageDialogClass dialog;
};

enum {
	PROP_0,
	PROP_CHECK_LABEL,
	PROP_CHECK_VALUE,
	PROP_CHECK_REQUIRE
};

G_DEFINE_TYPE (SeahorseDeleteDialog, seahorse_delete_dialog, GTK_TYPE_MESSAGE_DIALOG);

static void
seahorse_delete_dialog_init (SeahorseDeleteDialog *self)
{

}

static void
update_response_buttons (SeahorseDeleteDialog *self)
{
	gboolean active;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check));
	gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK,
	                                   !self->check_require || active);
}

static void
on_check_toggled (GtkToggleButton *toggle,
                  gpointer user_data)
{
	update_response_buttons (user_data);
}

static void
seahorse_delete_dialog_constructed (GObject *obj)
{
	SeahorseDeleteDialog *self = SEAHORSE_DELETE_DIALOG (obj);
	GtkWidget *content;
	GtkWidget *button;

	G_OBJECT_CLASS (seahorse_delete_dialog_parent_class)->constructed (obj);

	gtk_window_set_modal (GTK_WINDOW (self), TRUE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (self), TRUE);

	self->check = gtk_check_button_new ();
	content = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (self));
	gtk_container_add (GTK_CONTAINER (content), self->check);
	g_signal_connect (self->check, "toggled", G_CALLBACK (on_check_toggled), self);

	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget (GTK_DIALOG (self), button, GTK_RESPONSE_CANCEL);
	gtk_widget_show (button);

	button = gtk_button_new_from_stock (GTK_STOCK_DELETE);
	gtk_dialog_add_action_widget (GTK_DIALOG (self), button, GTK_RESPONSE_OK);
	gtk_widget_show (button);
}

static void
seahorse_delete_dialog_get_property (GObject *obj,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	SeahorseDeleteDialog *self = SEAHORSE_DELETE_DIALOG (obj);

	switch (prop_id) {
	case PROP_CHECK_LABEL:
		g_value_set_string (value, seahorse_delete_dialog_get_check_label (self));
		break;
	case PROP_CHECK_VALUE:
		g_value_set_boolean (value, seahorse_delete_dialog_get_check_value (self));
		break;
	case PROP_CHECK_REQUIRE:
		g_value_set_boolean (value, seahorse_delete_dialog_get_check_require (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_delete_dialog_set_property (GObject *obj,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	SeahorseDeleteDialog *self = SEAHORSE_DELETE_DIALOG (obj);

	switch (prop_id) {
	case PROP_CHECK_LABEL:
		seahorse_delete_dialog_set_check_label (self, g_value_get_string (value));
		break;
	case PROP_CHECK_VALUE:
		seahorse_delete_dialog_set_check_value (self, g_value_get_boolean (value));
		break;
	case PROP_CHECK_REQUIRE:
		seahorse_delete_dialog_set_check_require (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_delete_dialog_class_init (SeahorseDeleteDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_delete_dialog_constructed;
	gobject_class->get_property = seahorse_delete_dialog_get_property;
	gobject_class->set_property = seahorse_delete_dialog_set_property;

	g_object_class_install_property (gobject_class, PROP_CHECK_LABEL,
	            g_param_spec_string ("check-label", "Check label", "Check label",
	                                 NULL, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CHECK_VALUE,
	           g_param_spec_boolean ("check-value", "Check value", "Check value",
	                                 FALSE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CHECK_REQUIRE,
	           g_param_spec_boolean ("check-require", "Check require", "Check require",
	                                 FALSE, G_PARAM_READWRITE));
}

GtkDialog *
seahorse_delete_dialog_new (GtkWindow *parent,
                            const gchar *format,
                            ...)
{
	GtkDialog *dialog;
	gchar *message;
	va_list va;

	g_return_val_if_fail (format != NULL, NULL);

	va_start (va, format);
	message = g_strdup_vprintf (format, va);
	va_end (va);

	dialog = g_object_new (SEAHORSE_TYPE_DELETE_DIALOG,
	                       "message-type", GTK_MESSAGE_QUESTION,
	                       "transient-for", parent,
	                       "text", message,
	                       NULL);

	g_free (message);
	return dialog;
}

const gchar *
seahorse_delete_dialog_get_check_label (SeahorseDeleteDialog *self)
{
	g_return_val_if_fail (SEAHORSE_IS_DELETE_DIALOG (self), NULL);

	if (gtk_widget_get_visible (self->check))
		return gtk_button_get_label (GTK_BUTTON (self->check));
	return NULL;
}

void
seahorse_delete_dialog_set_check_label (SeahorseDeleteDialog *self,
                                        const gchar *label)
{
	g_return_if_fail (SEAHORSE_IS_DELETE_DIALOG (self));

	if (label == NULL) {
		gtk_widget_hide (self->check);
		label = "";
	} else {
		gtk_widget_show (self->check);
	}

	gtk_button_set_label (GTK_BUTTON (self->check), label);
	g_object_notify (G_OBJECT (self), "check-label");
}

gboolean
seahorse_delete_dialog_get_check_value (SeahorseDeleteDialog *self)
{
	g_return_val_if_fail (SEAHORSE_IS_DELETE_DIALOG (self), FALSE);

	if (gtk_widget_get_visible (self->check))
		return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check));
	return FALSE;
}

void
seahorse_delete_dialog_set_check_value (SeahorseDeleteDialog *self,
                                        gboolean value)
{
	g_return_if_fail (SEAHORSE_IS_DELETE_DIALOG (self));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->check), value);
	g_object_notify (G_OBJECT (self), "check-value");
}

gboolean
seahorse_delete_dialog_get_check_require (SeahorseDeleteDialog *self)
{
	g_return_val_if_fail (SEAHORSE_IS_DELETE_DIALOG (self), FALSE);
	return self->check_require;
}

void
seahorse_delete_dialog_set_check_require (SeahorseDeleteDialog *self,
                                          gboolean value)
{
	g_return_if_fail (SEAHORSE_IS_DELETE_DIALOG (self));
	self->check_require = value;
	update_response_buttons (self);
	g_object_notify (G_OBJECT (self), "check-require");
}

gboolean
seahorse_delete_dialog_prompt (GtkWindow *parent,
                               const gchar *text)
{
	GtkDialog *dialog;
	gint response;

	g_return_val_if_fail (text != NULL, FALSE);

	dialog = seahorse_delete_dialog_new (parent, "%s", text);

	response = gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	return (response == GTK_RESPONSE_OK);
}
