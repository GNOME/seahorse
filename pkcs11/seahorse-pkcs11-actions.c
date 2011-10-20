/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 */

#include "config.h"

#include "seahorse-pkcs11-actions.h"

#include "seahorse-certificate.h"
#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-certificate-props.h"
#include "seahorse-pkcs11-operations.h"

#include "seahorse-action.h"
#include "seahorse-actions.h"
#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

GType   seahorse_pkcs11_actions_get_type           (void) G_GNUC_CONST;
#define SEAHORSE_TYPE_PKCS11_ACTIONS               (seahorse_pkcs11_actions_get_type ())
#define SEAHORSE_PKCS11_ACTIONS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_ACTIONS, SeahorsePkcs11Actions))
#define SEAHORSE_PKCS11_ACTIONS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_ACTIONS, SeahorsePkcs11ActionsClass))
#define SEAHORSE_PKCS11_IS_ACTIONS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_ACTIONS))
#define SEAHORSE_PKCS11_IS_ACTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_ACTIONS))
#define SEAHORSE_PKCS11_ACTIONS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_ACTIONS, SeahorsePkcs11ActionsClass))

typedef struct {
	SeahorseActions parent;
} SeahorsePkcs11Actions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorsePkcs11ActionsClass;

static GQuark QUARK_WINDOW = 0;

G_DEFINE_TYPE (SeahorsePkcs11Actions, seahorse_pkcs11_actions, SEAHORSE_TYPE_ACTIONS);

static void
properties_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_show_properties (GtkAction *action,
                    gpointer user_data)
{
	GtkWindow *window;
	gpointer previous;
	GObject *object;

	object = seahorse_action_get_object (action);

	/* Try to show an already present window */
	previous = g_object_get_qdata (G_OBJECT (object), QUARK_WINDOW);
	if (GTK_IS_WINDOW (previous)) {
		window = GTK_WINDOW (previous);
		if (gtk_widget_get_visible (GTK_WIDGET (window))) {
			gtk_window_present (window);
			return;
		}
	}

	/* Create a new dialog for the certificate */
	window = GTK_WINDOW (seahorse_pkcs11_certificate_props_new (GCR_CERTIFICATE (object)));
	gtk_window_set_transient_for (window, seahorse_action_get_window (action));
	g_object_set_qdata (G_OBJECT (object), QUARK_WINDOW, window);
	gtk_widget_show (GTK_WIDGET (window));

	/* Close the window when we get a response */
	g_signal_connect (window, "response", G_CALLBACK (properties_response), NULL);
}

static void
on_delete_completed (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GError *error = NULL;

	if (!seahorse_pkcs11_delete_finish (result, &error))
		seahorse_util_handle_error (&error, parent, _("Couldn't delete"));

	g_object_unref (parent);
}

static void
on_delete_objects (GtkAction *action,
                   gpointer user_data)
{
	GCancellable *cancellable;
	gchar *prompt;
	const gchar *display;
	GtkWidget *parent;
	gboolean ret;
	guint num;
	GList *objects;

	objects = seahorse_action_get_objects (action);
	num = g_list_length (objects);
	if (num == 1) {
		display = seahorse_object_get_label (SEAHORSE_OBJECT (objects->data));
		prompt = g_strdup_printf (_("Are you sure you want to delete the certificate '%s'?"), display);
	} else {
		prompt = g_strdup_printf (ngettext (
				"Are you sure you want to delete %d certificate?",
				"Are you sure you want to delete %d certificates?",
				num), num);
	}

	parent = GTK_WIDGET (seahorse_action_get_window (action));
	ret = seahorse_util_prompt_delete (prompt, parent);
	g_free (prompt);

	if (ret) {
		cancellable = g_cancellable_new ();
		seahorse_pkcs11_delete_async (objects, cancellable,
		                              on_delete_completed, g_object_ref (parent));
		seahorse_progress_show (cancellable, _("Deleting"), TRUE);
		g_object_unref (cancellable);
	} else {
		seahorse_action_cancel (action);
	}
}

static const GtkActionEntry CERTIFICATE_ACTIONS[] = {
	{ "properties", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Properties of the certificate."), G_CALLBACK (on_show_properties) },
	{ "delete", GTK_STOCK_DELETE, NULL, NULL,
	  N_("Delete the certificate."), G_CALLBACK (on_delete_objects) },
};

static void
seahorse_pkcs11_actions_init (SeahorsePkcs11Actions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, CERTIFICATE_ACTIONS, G_N_ELEMENTS (CERTIFICATE_ACTIONS), self);
}

static void
seahorse_pkcs11_actions_class_init (SeahorsePkcs11ActionsClass *klass)
{
	QUARK_WINDOW = g_quark_from_static_string ("seahorse-pkcs11-actions-window");
}

GtkActionGroup *
seahorse_pkcs11_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_PKCS11_ACTIONS,
		                        "name", "pkcs11-certificate",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}
