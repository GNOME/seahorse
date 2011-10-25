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
#include "seahorse-gkr-dialogs.h"
#include "seahorse-gkr-operation.h"

#include "seahorse-action.h"
#include "seahorse-actions.h"
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
seahorse_gkr_backend_actions_init (SeahorseGkrBackendActions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);

	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
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
seahorse_gkr_backend_actions_class_init (SeahorseGkrBackendActionsClass *klass)
{

}

GtkActionGroup *
seahorse_gkr_backend_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_GKR_BACKEND_ACTIONS,
		                        "name", "KeyringBackend",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}

#define SEAHORSE_TYPE_GKR_KEYRING_ACTIONS            (seahorse_gkr_keyring_actions_get_type ())
#define SEAHORSE_GKR_KEYRING_ACTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_KEYRING_ACTIONS, SeahorseGkrKeyringActions))
#define SEAHORSE_GKR_KEYRING_ACTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_KEYRING_ACTIONS, SeahorseGkrKeyringActionsClass))
#define SEAHORSE_IS_GKR_KEYRING_ACTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_KEYRING_ACTIONS))
#define SEAHORSE_IS_GKR_KEYRING_ACTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_KEYRING_ACTIONS))
#define SEAHORSE_GKR_KEYRING_ACTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_KEYRING_ACTIONS, SeahorseGkrKeyringActionsClass))

typedef struct {
	SeahorseActions parent;
} SeahorseGkrKeyringActions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorseGkrKeyringActionsClass;

GType     seahorse_gkr_keyring_actions_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (SeahorseGkrKeyringActions, seahorse_gkr_keyring_actions, SEAHORSE_TYPE_ACTIONS);

static void
on_keyring_unlock_done (GnomeKeyringResult result,
                        gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		seahorse_util_show_error (parent, _("Couldn't unlock keyring"),
		                          gnome_keyring_result_to_message (result));
	}

	refresh_all_keyrings ();
}

static void
on_keyrings_unlock (GtkAction *action,
                    gpointer user_data)
{
	GList *keys = user_data;
	GtkWindow *parent;
	GList *l;

	parent = seahorse_action_get_window (action);
	for (l = keys; l; l = g_list_next (l)) {
		g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (l->data));
		gnome_keyring_unlock (seahorse_gkr_keyring_get_name (l->data), NULL,
		                      on_keyring_unlock_done, g_object_ref (parent), g_object_unref);
	}
}

static void
on_keyring_lock_done (GnomeKeyringResult result, gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		seahorse_util_show_error (parent, _("Couldn't lock keyring"),
		                          gnome_keyring_result_to_message (result));
	}

	refresh_all_keyrings ();
}

static void
on_keyrings_lock (GtkAction *action,
                  gpointer user_data)
{
	GList *keyrings = user_data;
	GtkWindow *parent;
	GList *l;

	parent = seahorse_action_get_window (action);
	for (l = keyrings; l; l = g_list_next (l)) {
		g_return_if_fail (SEAHORSE_IS_GKR_KEYRING (l->data));
		gnome_keyring_lock (seahorse_gkr_keyring_get_name (l->data),
		                    on_keyring_lock_done, g_object_ref (parent), g_object_unref);
	}
}

static void
on_set_default_keyring_done (GnomeKeyringResult result,
                             gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		seahorse_util_show_error (parent, _("Couldn't set default keyring"),
		                          gnome_keyring_result_to_message (result));
	}

	refresh_all_keyrings ();
}

static void
on_keyring_default (GtkAction *action,
                    gpointer user_data)
{
	SeahorseGkrKeyring *keyring = SEAHORSE_GKR_KEYRING (user_data);
	GtkWindow *parent = seahorse_action_get_window (action);
	gnome_keyring_set_default_keyring (seahorse_gkr_keyring_get_name (keyring),
	                                   on_set_default_keyring_done,
	                                   g_object_ref (parent), g_object_unref);
}

static void
on_change_password_done (GnomeKeyringResult result, gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);

	if (result != GNOME_KEYRING_RESULT_OK &&
	    result != GNOME_KEYRING_RESULT_DENIED &&
	    result != GNOME_KEYRING_RESULT_CANCELLED) {
		seahorse_util_show_error (parent, _("Couldn't change keyring password"),
		                          gnome_keyring_result_to_message (result));
	}

	refresh_all_keyrings ();
}

static void
on_keyring_password (GtkAction *action,
                     gpointer user_data)
{
	SeahorseGkrKeyring *keyring = SEAHORSE_GKR_KEYRING (user_data);
	GtkWindow *window = seahorse_action_get_window (action);

	gnome_keyring_change_password (seahorse_gkr_keyring_get_name (keyring),
	                               NULL, NULL, on_change_password_done,
	                               g_object_ref (window), g_object_unref);
}

static void
on_keyring_properties (GtkAction* action,
                       gpointer user_data)
{
	seahorse_gkr_keyring_properties_show (SEAHORSE_GKR_KEYRING (user_data),
	                                      seahorse_action_get_window (action));
}

static void
on_delete_objects_complete (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GtkWindow *parent = GTK_WINDOW (user_data);
	GError *error = NULL;

	if (!seahorse_gkr_delete_finish (result, &error)) {
		seahorse_util_show_error (parent, _("Couldn't delete keyring"), error->message);
		g_error_free (error);
	}

	g_object_unref (parent);
}

static void
on_keyrings_delete (GtkAction* action,
                    gpointer user_data)
{
	GList *objects = user_data;
	GCancellable *cancellable;
	GtkWindow *parent;
	gchar *prompt;
	gboolean ret;

	if (!objects)
		return;

	prompt = g_strdup_printf (_ ("Are you sure you want to delete the password keyring '%s'?"),
	                          seahorse_object_get_label (objects->data));
	parent = seahorse_action_get_window (action);

	ret = seahorse_util_prompt_delete (prompt, GTK_WIDGET (parent));
	if (ret) {
		cancellable = g_cancellable_new ();
		seahorse_gkr_delete_async (objects, cancellable,
		                           on_delete_objects_complete,
		                           g_object_ref (parent));
		seahorse_progress_show (cancellable, ngettext ("Deleting keyring", "Deleting keyrings",
		                                               g_list_length (objects)), TRUE);
		g_object_unref (cancellable);
	} else {
		g_cancellable_cancel (g_cancellable_get_current ());
	}

	g_free (prompt);
}

static const GtkActionEntry KEYRINGS_ACTIONS[] = {
	{ "keyring-lock", NULL, N_("_Lock"), "",
	  N_("Lock the password storage keyring so a master password is required to unlock it."), G_CALLBACK (on_keyrings_lock) },
	{ "keyring-unlock", NULL, N_("_Unlock"), "",
	  N_("Unlock the password storage keyring with a master password so it is available for use."), G_CALLBACK (on_keyrings_unlock) },
	{ "keyring-delete", GTK_STOCK_DELETE, NULL, NULL,
	  N_("Delete the keyring."), G_CALLBACK (on_keyrings_delete) },
};

static const GtkActionEntry KEYRING_ACTIONS[] = {
	{ "keyring-default", NULL, N_("_Set as default"), "",
	  N_("Applications usually store new passwords in the default keyring."), G_CALLBACK (on_keyring_default) },
	{ "keyring-password", NULL, N_("Change _Password"), "",
	  N_("Change the unlock password of the password storage keyring"), G_CALLBACK (on_keyring_password) },
	{ "keyring-properties", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Properties of the keyring."), G_CALLBACK (on_keyring_properties) },
};

static const gchar* KEYRING_UI =
"<ui>"
"	<popup name='SeahorseGkrKeyring'>"
"		<menuitem action='keyring-unlock'/>"
"		<menuitem action='keyring-lock'/>"
"		<menuitem action='keyring-default'/>"
"		<menuitem action='keyring-password'/>"
"		<menuitem action='keyring-delete'/>"
"		<menuitem action='keyring-properties'/>"
"	</popup>"\
"</ui>";

static void
seahorse_gkr_keyring_actions_init (SeahorseGkrKeyringActions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);

	/* Add these actions, but none of them are visible until cloned */
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, KEYRINGS_ACTIONS, G_N_ELEMENTS (KEYRINGS_ACTIONS), NULL);
	gtk_action_group_add_actions (actions, KEYRING_ACTIONS, G_N_ELEMENTS (KEYRING_ACTIONS), NULL);
	gtk_action_group_set_sensitive (actions, FALSE);

	seahorse_actions_register_definition (SEAHORSE_ACTIONS (self), KEYRING_UI);
}

static GtkActionGroup *
seahorse_gkr_keyring_actions_clone_for_objects (SeahorseActions *actions,
                                                GList *objects)
{
	GnomeKeyringInfo *info;
	gboolean locked = FALSE;
	gboolean unlocked = FALSE;
	gboolean can_default = FALSE;
	GtkActionGroup *cloned;
	GList *l;

	g_return_val_if_fail (objects != NULL, NULL);

	for (l = objects; l; l = g_list_next (l)) {
		info = seahorse_gkr_keyring_get_info (l->data);
		if (info != NULL) {
			if (gnome_keyring_info_get_is_locked (info))
				locked = TRUE;
			else
				unlocked = TRUE;
			if (!seahorse_gkr_keyring_get_is_default (l->data))
				can_default = TRUE;
		}
	}

	cloned = gtk_action_group_new ("KeyringObject");
	gtk_action_group_set_translation_domain (cloned, GETTEXT_PACKAGE);

	gtk_action_group_add_actions_full (cloned, KEYRINGS_ACTIONS,
	                                   G_N_ELEMENTS (KEYRINGS_ACTIONS),
	                                   seahorse_object_list_copy (objects),
	                                   seahorse_object_list_free);
	gtk_action_set_sensitive (gtk_action_group_get_action (cloned, "keyring-lock"), unlocked);
	gtk_action_set_sensitive (gtk_action_group_get_action (cloned, "keyring-unlock"), locked);

	/* Single object */
	if (!objects->next) {
		gtk_action_group_add_actions_full (cloned, KEYRING_ACTIONS,
		                                   G_N_ELEMENTS (KEYRING_ACTIONS),
		                                   g_object_ref (objects->data),
		                                   g_object_unref);
		gtk_action_set_sensitive (gtk_action_group_get_action (cloned, "keyring-default"), can_default);
	}

	return cloned;
}

static void
seahorse_gkr_keyring_actions_class_init (SeahorseGkrKeyringActionsClass *klass)
{
	SeahorseActionsClass *actions_class = SEAHORSE_ACTIONS_CLASS (klass);
	actions_class->clone_for_objects = seahorse_gkr_keyring_actions_clone_for_objects;
}

GtkActionGroup *
seahorse_gkr_keyring_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_GKR_KEYRING_ACTIONS,
		                        "name", "gkr-keyring",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}

GType   seahorse_gkr_item_actions_get_type        (void);
#define SEAHORSE_TYPE_GKR_ITEM_ACTIONS            (seahorse_gkr_item_actions_get_type ())
#define SEAHORSE_GKR_ITEM_ACTIONS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GKR_ITEM_ACTIONS, SeahorseGkrItemActions))
#define SEAHORSE_GKR_ITEM_ACTIONS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GKR_ITEM_ACTIONS, SeahorseGkrItemActionsClass))
#define SEAHORSE_IS_GKR_ITEM_ACTIONS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GKR_ITEM_ACTIONS))
#define SEAHORSE_IS_GKR_ITEM_ACTIONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GKR_ITEM_ACTIONS))
#define SEAHORSE_GKR_ITEM_ACTIONS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GKR_ITEM_ACTIONS, SeahorseGkrItemActionsClass))

typedef struct {
	SeahorseActions parent;
} SeahorseGkrItemActions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorseGkrItemActionsClass;

G_DEFINE_TYPE (SeahorseGkrItemActions, seahorse_gkr_item_actions, SEAHORSE_TYPE_ACTIONS);

static void
on_password_properties (GtkAction *action,
                        gpointer user_data)
{
	seahorse_gkr_item_properties_show (SEAHORSE_GKR_ITEM (user_data),
	                                   seahorse_action_get_window (action));
}

static void
on_delete_objects (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
	GtkWidget *parent = GTK_WIDGET (user_data);
	GError *error = NULL;

	if (!seahorse_gkr_delete_finish (result, &error)) {
		seahorse_util_show_error (parent, _("Couldn't delete item"), error->message);
		g_error_free (error);
	}

	g_object_unref (parent);
}

static void
on_delete_passwords (GtkAction *action,
                     gpointer user_data)
{
	GCancellable *cancellable;
	GtkWidget *parent;
	GList *objects;
	gchar *prompt;
	gboolean ret;
	guint num;

	objects = user_data;
	if (objects == NULL)
		return;

	num = g_list_length (objects);
	if (num == 1) {
		prompt = g_strdup_printf (_ ("Are you sure you want to delete the password '%s'?"),
		                          seahorse_object_get_label (SEAHORSE_OBJECT (objects->data)));
	} else {
		prompt = g_strdup_printf (ngettext ("Are you sure you want to delete %d password?",
		                                    "Are you sure you want to delete %d passwords?",
		                                    num), num);
	}

	parent = GTK_WIDGET (seahorse_action_get_window (action));
	ret = seahorse_util_prompt_delete (prompt, parent);

	if (ret) {
		cancellable = g_cancellable_new ();
		seahorse_gkr_delete_async (objects, cancellable, on_delete_objects, g_object_ref (parent));
		seahorse_progress_show (cancellable, ngettext ("Deleting item", "Deleting items", num), TRUE);
		g_object_unref (cancellable);
	} else {
		g_cancellable_cancel (g_cancellable_get_current ());

	}

	g_free (prompt);
}

static const GtkActionEntry ITEM_ACTIONS[] = {
	{ "properties", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Properties of the password."), G_CALLBACK (on_password_properties) },
};

static const GtkActionEntry ITEMS_ACTIONS[] = {
	{ "delete", GTK_STOCK_DELETE, NULL, NULL,
	  N_("Delete the password."), G_CALLBACK (on_delete_passwords) },
};

static void
seahorse_gkr_item_actions_init (SeahorseGkrItemActions *self)
{

}

static GtkActionGroup *
seahorse_gkr_item_actions_clone_for_objects (SeahorseActions *actions,
                                             GList *objects)
{
	GtkActionGroup *cloned;

	cloned = gtk_action_group_new ("KeyringItem");
	gtk_action_group_set_translation_domain (cloned, GETTEXT_PACKAGE);
	gtk_action_group_add_actions_full (cloned, ITEMS_ACTIONS,
	                                   G_N_ELEMENTS (ITEMS_ACTIONS),
	                                   seahorse_object_list_copy (objects),
	                                   seahorse_object_list_free);

	if (!objects->next)
		gtk_action_group_add_actions_full (cloned, ITEM_ACTIONS,
		                                   G_N_ELEMENTS (ITEM_ACTIONS),
		                                   g_object_ref (objects->data),
		                                   g_object_unref);

	return cloned;
}

static void
seahorse_gkr_item_actions_class_init (SeahorseGkrItemActionsClass *klass)
{
	SeahorseActionsClass *actions_class = SEAHORSE_ACTIONS_CLASS (klass);
	actions_class->clone_for_objects = seahorse_gkr_item_actions_clone_for_objects;
}

GtkActionGroup *
seahorse_gkr_item_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_GKR_ITEM_ACTIONS,
		                        "name", "KeyringItem",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}
