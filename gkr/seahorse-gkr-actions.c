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

#include "seahorse-gkr-actions.h"
#include "seahorse-gkr-backend.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-gkr-keyring-deleter.h"
#include "seahorse-gkr-dialogs.h"

#include "seahorse-action.h"
#include "seahorse-actions.h"
#include "seahorse-delete-dialog.h"
#include "seahorse-object-list.h"
#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

GType   seahorse_gkr_backend_actions_get_type           (void) G_GNUC_CONST;
#define SEAHORSE_TYPE_GKR_BACKEND_ACTIONS               (seahorse_gkr_backend_actions_get_type ())
#define SEAHORSE_GKR_BACKEND_ACTIONS(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_BACKEND_ACTIONS, SeahorseGkrBackendActions))
#define SEAHORSE_GKR_BACKEND_ACTIONS_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_BACKEND_ACTIONS, SeahorseGkrBackendActionsClass))
#define SEAHORSE_IS_GKR_BACKEND_ACTIONS(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_BACKEND_ACTIONS))
#define SEAHORSE_IS_GKR_BACKEND_ACTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_BACKEND_ACTIONS))
#define SEAHORSE_GKR_BACKEND_ACTIONS_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_BACKEND_ACTIONS, SeahorseGkrBackendActionsClass))

typedef struct {
	SeahorseActions parent;
	gboolean initialized;
} SeahorseGkrBackendActions;

typedef struct {
	SeahorseActionsClass parent;
} SeahorseGkrBackendActionsClass;

G_DEFINE_TYPE (SeahorseGkrBackendActions, seahorse_gkr_backend_actions, SEAHORSE_TYPE_ACTIONS);

static void
refresh_all_keyrings (void)
{
	seahorse_gkr_backend_load_async (NULL, NULL, NULL, NULL);
}

static void
on_new_keyring (GtkAction *action,
                gpointer unused)
{
	seahorse_gkr_add_keyring_show (seahorse_action_get_window (action));
}

static void
on_new_item (GtkAction *action,
             gpointer unused)
{
	seahorse_gkr_add_item_show (seahorse_action_get_window (action));
}

static const GtkActionEntry BACKEND_ACTIONS[] = {
	{ "keyring-new", NULL, N_("New password keyring"), "",
	  N_("Used to store application and network passwords"), G_CALLBACK (on_new_keyring) },
	{ "keyring-item-new", NULL, N_("New password..."), "",
	  N_("Safely store a password or secret."), G_CALLBACK (on_new_item) },
};

static const GtkActionEntry ENTRIES_NEW[] = {
	{ "keyring-new", "folder", N_("Password Keyring"), "",
	  N_("Used to store application and network passwords"), G_CALLBACK (on_new_keyring) },
	{ "keyring-item-new", GCR_ICON_PASSWORD, N_("Stored Password"), "",
	  N_("Safely store a password or secret."), G_CALLBACK (on_new_item) }
};

static const gchar* BACKEND_UI =
"<ui>"
"	<popup name='SeahorseGkrBackend'>"
"		<menuitem action='keyring-new'/>"
"	</popup>"\
"</ui>";

static void
on_backend_notify_service (GObject *obj,
                           GParamSpec *pspec,
                           gpointer user_data)
{
	SeahorseGkrBackendActions *self = SEAHORSE_GKR_BACKEND_ACTIONS (user_data);
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);

	if (self->initialized)
		return;
	if (seahorse_gkr_backend_get_service (SEAHORSE_GKR_BACKEND (obj)) == NULL)
		return;

	self->initialized = TRUE;
	gtk_action_group_add_actions (actions, BACKEND_ACTIONS, G_N_ELEMENTS (BACKEND_ACTIONS), NULL);
	seahorse_actions_register_definition (SEAHORSE_ACTIONS (self), BACKEND_UI);

	/* Register another set of actions as a generator */
	actions = gtk_action_group_new ("gkr-generate");
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, ENTRIES_NEW, G_N_ELEMENTS (ENTRIES_NEW), NULL);
	seahorse_registry_register_object (NULL, G_OBJECT (actions), "generator", NULL);
	g_object_unref (actions);
}

static void
seahorse_gkr_backend_actions_init (SeahorseGkrBackendActions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
}

static void
seahorse_gkr_backend_actions_class_init (SeahorseGkrBackendActionsClass *klass)
{

}

GtkActionGroup *
seahorse_gkr_backend_actions_instance (SeahorseGkrBackend *backend)
{
	static SeahorseGkrBackendActions *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_GKR_BACKEND_ACTIONS,
		                        "name", "KeyringBackend",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);

		g_signal_connect_object (backend, "notify::service",
		                         G_CALLBACK (on_backend_notify_service),
		                         actions, G_CONNECT_AFTER);

		if (seahorse_gkr_backend_get_service (backend) != NULL)
			on_backend_notify_service (G_OBJECT (backend), NULL, actions);
	} else {
		g_object_ref (actions);
	}

	return GTK_ACTION_GROUP (actions);
}

static void
on_set_default_keyring_done (GObject *source,
                             GAsyncResult *result,
                             gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GError *error = NULL;

	secret_service_set_alias_finish (SECRET_SERVICE (source), result, &error);
	if (error != NULL)
		seahorse_util_handle_error (&error, parent, _("Couldn't set default keyring"));

	refresh_all_keyrings ();
	if (parent)
		g_object_unref (parent);
}

static void
on_keyring_default (GtkAction *action,
                    gpointer user_data)
{
	SecretCollection *keyring = SECRET_COLLECTION (user_data);
	GtkWindow *parent = seahorse_action_get_window (action);

	if (parent != NULL)
		g_object_ref (parent);

	secret_service_set_alias (secret_collection_get_service (keyring), "default",
	                          SECRET_COLLECTION (keyring),
	                          NULL, on_set_default_keyring_done, parent);
}

typedef struct {
	SecretService *service;
	GtkWindow *parent;
} ChangeClosure;

static void
change_closure_free (gpointer data)
{
	ChangeClosure *change = data;
	g_object_unref (change->service);
	if (change->parent)
		g_object_unref (change->parent);
	g_slice_free (ChangeClosure, change);
}

static void
on_change_password_done (GObject *source,
                         GAsyncResult *result,
                         gpointer user_data)
{
	ChangeClosure *change = user_data;
	GError *error = NULL;
	GVariant *retval;

	retval = secret_service_prompt_at_dbus_path_finish (SECRET_SERVICE (source), result, NULL, &error);
	if (retval != NULL)
		g_variant_unref (retval);
	if (error != NULL)
		seahorse_util_handle_error (&error, change->parent,
		                            _("Couldn't change keyring password"));

	refresh_all_keyrings ();
	change_closure_free (change);
}

static void
on_change_password_prompt (GObject *source,
                           GAsyncResult *result,
                           gpointer user_data)
{
	ChangeClosure *change = user_data;
	const gchar *prompt_path;
	GError *error = NULL;
	GVariant *retval;

	retval = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
	if (error == NULL) {
		g_variant_get (retval, "(&o)", &prompt_path);
		secret_service_prompt_at_dbus_path (change->service, prompt_path, NULL,
		                                    on_change_password_done, change);
		g_variant_unref (retval);
	} else {
		seahorse_util_handle_error (&error, change->parent, _("Couldn't change keyring password"));
		change_closure_free (change);
	}
}

static void
on_keyring_password (GtkAction *action,
                     gpointer user_data)
{
	SecretCollection *collection = SECRET_COLLECTION (user_data);
	GDBusProxy *proxy;
	ChangeClosure *change;

	change = g_slice_new0 (ChangeClosure);
	change->parent = seahorse_action_get_window (action);
	if (change->parent)
		g_object_ref (change->parent);
	change->service = secret_collection_get_service (collection);
	g_return_if_fail (change->service != NULL);
	g_object_ref (change->service);

	proxy = G_DBUS_PROXY (change->service);
	g_dbus_connection_call (g_dbus_proxy_get_connection (proxy),
	                        g_dbus_proxy_get_name (proxy),
	                        g_dbus_proxy_get_object_path (proxy),
	                        "org.gnome.keyring.InternalUnsupportedGuiltRiddenInterface",
	                        "ChangeWithPrompt",
	                        g_variant_new ("(o)", g_dbus_proxy_get_object_path (G_DBUS_PROXY (collection))),
	                        G_VARIANT_TYPE ("(o)"),
	                        0, -1, NULL, on_change_password_prompt, change);
}

static const GtkActionEntry KEYRING_ACTIONS[] = {
	{ "keyring-default", NULL, N_("_Set as default"), NULL,
	  N_("Applications usually store new passwords in the default keyring."), G_CALLBACK (on_keyring_default) },
	{ "keyring-password", NULL, N_("Change _Password"), NULL,
	  N_("Change the unlock password of the password storage keyring"), G_CALLBACK (on_keyring_password) },
};

GtkActionGroup *
seahorse_gkr_keyring_actions_new (SeahorseGkrKeyring *keyring)
{
	GtkActionGroup *actions;
	GtkAction *action;

	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (keyring), NULL);

	actions = gtk_action_group_new ("KeyringActions");

	/* Add these actions, but none of them are visible until cloned */
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions_full (actions, KEYRING_ACTIONS, G_N_ELEMENTS (KEYRING_ACTIONS),
	                                   g_object_ref (keyring), g_object_unref);

	action = gtk_action_group_get_action (actions, "keyring-default");
	g_object_bind_property (keyring, "is-default", action, "sensitive",
	                        G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);

	return actions;
}
