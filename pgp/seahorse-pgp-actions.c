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

#include <glib/gi18n.h>

#include "seahorse-gpgme-dialogs.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-uid.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-pgp-actions.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-keyserver-search.h"
#include "seahorse-keyserver-sync.h"

#include "seahorse-action.h"
#include "seahorse-actions.h"
#include "seahorse-object.h"
#include "seahorse-object-list.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

GType   seahorse_pgp_backend_actions_get_type         (void) G_GNUC_CONST;
#define SEAHORSE_TYPE_PGP_BACKEND_ACTIONS             (seahorse_pgp_backend_actions_get_type ())
#define SEAHORSE_PGP_BACKEND_ACTIONS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_BACKEND_ACTIONS, SeahorsePgpBackendActions))
#define SEAHORSE_PGP_BACKEND_ACTIONS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_BACKEND_ACTIONS, SeahorsePgpBackendActionsClass))
#define SEAHORSE_IS_PGP_BACKEND_ACTIONS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_BACKEND_ACTIONS))
#define SEAHORSE_IS_PGP_BACKEND_ACTIONS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_BACKEND_ACTIONS))
#define SEAHORSE_PGP_BACKEND_ACTIONS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_BACKEND_ACTIONS, SeahorsePgpBackendActionsClass))

typedef struct {
	SeahorseActions parent_instance;
} SeahorsePgpBackendActions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorsePgpBackendActionsClass;

G_DEFINE_TYPE (SeahorsePgpBackendActions, seahorse_pgp_backend_actions, SEAHORSE_TYPE_ACTIONS);

static const gchar* BACKEND_DEFINITION = ""\
"<ui>"\
"	<menubar>"\
"		<placeholder name='RemoteMenu'>"\
"			<menu name='Remote' action='remote-menu'>"\
"				<menuitem action='remote-find'/>"\
"				<menuitem action='remote-sync'/>"\
"			</menu>"\
"		</placeholder>"\
"	</menubar>"\
"</ui>";

static void
on_remote_find (GtkAction* action,
                gpointer user_data)
{
	seahorse_keyserver_search_show (seahorse_action_get_window (action));
}

static void
on_remote_sync (GtkAction* action,
                gpointer user_data)
{
	SeahorseGpgmeKeyring *keyring;
	GList* objects = user_data;

	if (objects == NULL) {
		keyring = seahorse_pgp_backend_get_default_keyring (NULL);
		objects = gcr_collection_get_objects (GCR_COLLECTION (keyring));
	} else {
		objects = g_list_copy (objects);
	}

	seahorse_keyserver_sync_show (objects, seahorse_action_get_window (action));
	g_list_free (objects);
}

static const GtkActionEntry FIND_ACTIONS[] = {
	{ "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
	  N_("Search for keys on a key server"), G_CALLBACK (on_remote_find) },
};

static const GtkActionEntry SYNC_ACTIONS[] = {
	{ "remote-sync", GTK_STOCK_REFRESH, N_("_Sync and Publish Keys..."), "",
	  N_("Publish and/or synchronize your keys with those online."), G_CALLBACK (on_remote_sync) }
};

static void
seahorse_pgp_backend_actions_init (SeahorsePgpBackendActions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, FIND_ACTIONS,
	                              G_N_ELEMENTS (FIND_ACTIONS), NULL);
	gtk_action_group_add_actions (actions, SYNC_ACTIONS,
	                              G_N_ELEMENTS (SYNC_ACTIONS), NULL);
	seahorse_actions_register_definition (SEAHORSE_ACTIONS (self), BACKEND_DEFINITION);
}

static void
seahorse_pgp_backend_actions_class_init (SeahorsePgpBackendActionsClass *klass)
{

}

GtkActionGroup *
seahorse_pgp_backend_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_PGP_BACKEND_ACTIONS,
		                        "name", "pgp-backend",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}

GType   seahorse_gpgme_key_actions_get_type       (void) G_GNUC_CONST;
#define SEAHORSE_TYPE_GPGME_KEY_ACTIONS           (seahorse_gpgme_key_actions_get_type ())
#define seahorse_gpgme_key_actions(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PGP_ACTIONS, SeahorseGpgmeKeyActions))
#define seahorse_gpgme_key_actions_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PGP_ACTIONS, SeahorseGpgmeKeyActionsClass))
#define SEAHORSE_IS_PGP_ACTIONS(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PGP_ACTIONS))
#define SEAHORSE_IS_PGP_ACTIONS_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PGP_ACTIONS))
#define seahorse_gpgme_key_actions_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PGP_ACTIONS, SeahorseGpgmeKeyActionsClass))

typedef struct {
	SeahorseActions parent_instance;
} SeahorseGpgmeKeyActions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorseGpgmeKeyActionsClass;

G_DEFINE_TYPE (SeahorseGpgmeKeyActions, seahorse_gpgme_key_actions, SEAHORSE_TYPE_ACTIONS);

static const char* KEY_DEFINITION = ""\
"<ui>"\
"	<menubar>"\
"		<menu name='File' action='file-menu'>"\
"			<placeholder name='FileActions'>"\
"				<menuitem action='key-sign'/>"\
"			</placeholder>"\
"		</menu>"\
"	</menubar>"\
"	<popup name='ObjectPopup'>"\
"		<menuitem action='key-sign'/>"\
"	</popup>"\
"</ui>";

static void
on_key_sign (GtkAction* action,
             gpointer user_data)
{
	GtkWindow *window;
	GList *objects = user_data;

	g_return_if_fail (objects->data);

	window = seahorse_action_get_window (action);

	seahorse_gpgme_sign_prompt (SEAHORSE_GPGME_KEY (objects->data), window);
}

static void
on_show_properties (GtkAction *action,
                    gpointer user_data)
{
	seahorse_pgp_key_properties_show (SEAHORSE_PGP_KEY (user_data),
	                                  seahorse_action_get_window (action));
}

static void
on_delete_objects (GtkAction *action,
                   gpointer user_data)
{
	guint num;
	gint num_keys;
	gint num_identities;
	char* message;
	SeahorseObject *obj;
	GList* to_delete, *l;
	GtkWidget *parent;
	gpgme_error_t gerr;
	guint length;
	GError *error = NULL;
	GList *objects;

	objects = user_data;
	num = g_list_length (objects);
	if (num == 0)
		return;

	num_keys = 0;
	num_identities = 0;
	message = NULL;

	/*
	 * Go through and validate all what we have to delete,
	 * removing UIDs where the parent Key is also on the
	 * chopping block.
	 */
	to_delete = NULL;

	for (l = objects; l; l = g_list_next (l)) {
		obj = SEAHORSE_OBJECT (l->data);
		if (SEAHORSE_IS_PGP_UID (obj)) {
			if (g_list_find (objects, seahorse_pgp_uid_get_parent (SEAHORSE_PGP_UID (obj))) == NULL) {
				to_delete = g_list_prepend (to_delete, obj);
				++num_identities;
			}
		} else if (G_OBJECT_TYPE (obj) == SEAHORSE_TYPE_GPGME_KEY) {
			to_delete = g_list_prepend (to_delete, obj);
			++num_keys;
		}
	}

	/* Figure out a good prompt message */
	length = g_list_length (to_delete);
	switch (length) {
	case 0:
		return;
	case 1:
		message = g_strdup_printf (_ ("Are you sure you want to permanently delete %s?"),
		                           seahorse_object_get_label (to_delete->data));
		break;
	default:
		if (num_keys > 0 && num_identities > 0) {
			message = g_strdup_printf (_("Are you sure you want to permanently delete %d keys and identities?"), length);
		} else if (num_keys > 0) {
			message = g_strdup_printf (_("Are you sure you want to permanently delete %d keys?"), length);
		} else if (num_identities > 0){
			message = g_strdup_printf (_("Are you sure you want to permanently delete %d identities?"), length);
		} else {
			g_assert_not_reached ();
		}
		break;
	}

	parent = GTK_WIDGET (seahorse_action_get_window (action));
	if (!seahorse_util_prompt_delete (message, parent)) {
		g_free (message);
		g_cancellable_cancel (g_cancellable_get_current ());
		return;
	}

	gerr = 0;
	for (l = objects; l; l = g_list_next (l)) {
		obj = SEAHORSE_OBJECT (l->data);
		if (SEAHORSE_IS_GPGME_UID (obj)) {
			gerr = seahorse_gpgme_key_op_del_uid (SEAHORSE_GPGME_UID (obj));
			message = _("Couldn't delete user ID");
		} else if (SEAHORSE_IS_GPGME_KEY (obj)) {
			if (seahorse_object_get_usage (obj) == SEAHORSE_USAGE_PRIVATE_KEY) {
				gerr = seahorse_gpgme_key_op_delete_pair (SEAHORSE_GPGME_KEY (obj));
				message = _("Couldn't delete private key");
			} else {
				gerr = seahorse_gpgme_key_op_delete (SEAHORSE_GPGME_KEY (obj));
				message = _("Couldn't delete public key");
			}
		}

		if (seahorse_gpgme_propagate_error (gerr, &error)) {
			seahorse_util_handle_error (&error, parent, "%s", message);
			return;
		}
	}
}

static const GtkActionEntry KEY_ACTIONS[] = {
	{ "key-sign", GTK_STOCK_INDEX, N_("_Sign Key..."), "",
	  N_("Sign public key"), G_CALLBACK (on_key_sign) },
	{ "properties", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Properties of the key."), G_CALLBACK (on_show_properties) },
};

static const GtkActionEntry KEYS_ACTIONS[] = {
	{ "delete", GTK_STOCK_DELETE, NULL, NULL,
	  N_("Delete the key."), G_CALLBACK (on_delete_objects) },
};

static void
seahorse_gpgme_key_actions_init (SeahorseGpgmeKeyActions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, KEY_ACTIONS,
	                              G_N_ELEMENTS (KEY_ACTIONS), NULL);
	gtk_action_group_add_actions (actions, KEYS_ACTIONS,
	                              G_N_ELEMENTS (KEYS_ACTIONS), NULL);
	gtk_action_group_set_visible (actions, FALSE);
	seahorse_actions_register_definition (SEAHORSE_ACTIONS (self), KEY_DEFINITION);

}

static GtkActionGroup *
seahorse_gpgme_key_actions_clone_for_objects (SeahorseActions *actions,
                                              GList *objects)
{
	GtkActionGroup *cloned;

	g_return_val_if_fail (objects != NULL, NULL);

	cloned = gtk_action_group_new ("GpgmeKey");
	gtk_action_group_add_actions_full (cloned, KEYS_ACTIONS,
	                                   G_N_ELEMENTS (KEYS_ACTIONS),
	                                   seahorse_object_list_copy (objects),
	                                   seahorse_object_list_free);
	gtk_action_group_add_actions_full (cloned, SYNC_ACTIONS,
	                                   G_N_ELEMENTS (SYNC_ACTIONS),
	                                   seahorse_object_list_copy (objects),
	                                   seahorse_object_list_free);

	/* Single key */
	if (!objects->next) {
		gtk_action_group_add_actions_full (cloned, KEY_ACTIONS,
		                                   G_N_ELEMENTS (KEY_ACTIONS),
		                                   g_object_ref (objects->data),
		                                   g_object_unref);
	}

	return cloned;
}

static void
seahorse_gpgme_key_actions_class_init (SeahorseGpgmeKeyActionsClass *klass)
{
	SeahorseActionsClass *actions_class = SEAHORSE_ACTIONS_CLASS (klass);
	actions_class->clone_for_objects = seahorse_gpgme_key_actions_clone_for_objects;
}

GtkActionGroup *
seahorse_gpgme_key_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_GPGME_KEY_ACTIONS,
		                        "name", "gpgme-key",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}
