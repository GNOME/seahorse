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
#include "seahorse-common.h"
#include "seahorse-object.h"
#include "seahorse-object-list.h"
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

#ifdef WITH_KEYSERVER

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
	SeahorseActions *actions = SEAHORSE_ACTIONS (user_data);
	SeahorseGpgmeKeyring *keyring;
	SeahorseCatalog *catalog;
	GList *objects = NULL;
	GList *keys = NULL;
	GList *l;

	catalog = seahorse_actions_get_catalog (actions);
	if (catalog != NULL) {
		objects = seahorse_catalog_get_selected_objects (catalog);
		for (l = objects; l != NULL; l = g_list_next (l)) {
			if (SEAHORSE_IS_PGP_KEY (l->data))
				keys = g_list_prepend (keys, l->data);
		}
		g_list_free (objects);
	}

	if (keys == NULL) {
		keyring = seahorse_pgp_backend_get_default_keyring (NULL);
		keys = gcr_collection_get_objects (GCR_COLLECTION (keyring));
	}

	seahorse_keyserver_sync_show (keys, seahorse_action_get_window (action));
	g_list_free (keys);
}

static const GtkActionEntry FIND_ACTIONS[] = {
	{ "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
	  N_("Search for keys on a key server"), G_CALLBACK (on_remote_find) },
};

static const GtkActionEntry SYNC_ACTIONS[] = {
	{ "remote-sync", GTK_STOCK_REFRESH, N_("_Sync and Publish Keys..."), "",
	  N_("Publish and/or synchronize your keys with those online."), G_CALLBACK (on_remote_sync) }
};

#endif /* WITH_KEYSERVER */

static void
seahorse_pgp_backend_actions_init (SeahorsePgpBackendActions *self)
{
#ifdef WITH_KEYSERVER
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, FIND_ACTIONS,
	                              G_N_ELEMENTS (FIND_ACTIONS), NULL);
	gtk_action_group_add_actions (actions, SYNC_ACTIONS,
	                              G_N_ELEMENTS (SYNC_ACTIONS), self);
	seahorse_actions_register_definition (SEAHORSE_ACTIONS (self), BACKEND_DEFINITION);
#endif
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

static void
seahorse_gpgme_key_actions_init (SeahorseGpgmeKeyActions *self)
{
#ifdef WITH_KEYSERVER
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, SYNC_ACTIONS,
	                              G_N_ELEMENTS (SYNC_ACTIONS), NULL);
#endif
}

static void
seahorse_gpgme_key_actions_class_init (SeahorseGpgmeKeyActionsClass *klass)
{

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
