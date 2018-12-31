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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
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

#include "seahorse-common.h"

#include "libseahorse/seahorse-object-list.h"
#include "libseahorse/seahorse-util.h"

GType   seahorse_pgp_backend_actions_get_type         (void) G_GNUC_CONST;
#define SEAHORSE_PGP_TYPE_BACKEND_ACTIONS             (seahorse_pgp_backend_actions_get_type ())
#define SEAHORSE_PGP_BACKEND_ACTIONS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PGP_TYPE_BACKEND_ACTIONS, SeahorsePgpBackendActions))
#define SEAHORSE_PGP_BACKEND_ACTIONS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PGP_TYPE_BACKEND_ACTIONS, SeahorsePgpBackendActionsClass))
#define SEAHORSE_PGP_IS_BACKEND_ACTIONS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PGP_TYPE_BACKEND_ACTIONS))
#define SEAHORSE_PGP_IS_BACKEND_ACTIONS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PGP_TYPE_BACKEND_ACTIONS))
#define SEAHORSE_PGP_BACKEND_ACTIONS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PGP_TYPE_BACKEND_ACTIONS, SeahorsePgpBackendActionsClass))

typedef struct {
	SeahorseActionGroup parent_instance;
} SeahorsePgpBackendActions;

typedef struct {
	SeahorseActionGroupClass parent_class;
} SeahorsePgpBackendActionsClass;

G_DEFINE_TYPE (SeahorsePgpBackendActions, seahorse_pgp_backend_actions, SEAHORSE_TYPE_ACTION_GROUP);

#ifdef WITH_KEYSERVER

static void
on_remote_find (GSimpleAction *action,
                GVariant *param,
                gpointer user_data)
{
	SeahorseActionGroup *actions = SEAHORSE_ACTION_GROUP (user_data);
	SeahorseCatalog *catalog;

	catalog = seahorse_action_group_get_catalog (actions);
	seahorse_keyserver_search_show (GTK_WINDOW (catalog));
	g_clear_object (&catalog);
}

static void
on_remote_sync (GSimpleAction *action,
                GVariant *param,
                gpointer user_data)
{
	SeahorseActionGroup *actions = SEAHORSE_ACTION_GROUP (user_data);
	SeahorseGpgmeKeyring *keyring;
	SeahorseCatalog *catalog;
	GList *objects = NULL;
	GList *keys = NULL;
	GList *l;

	catalog = seahorse_action_group_get_catalog (actions);
	if (catalog != NULL) {
		objects = seahorse_catalog_get_selected_objects (catalog);
		for (l = objects; l != NULL; l = g_list_next (l)) {
			if (SEAHORSE_PGP_IS_KEY (l->data))
				keys = g_list_prepend (keys, l->data);
		}
		g_list_free (objects);
	}
	g_object_unref (catalog);

	if (keys == NULL) {
		keyring = seahorse_pgp_backend_get_default_keyring (NULL);
		keys = gcr_collection_get_objects (GCR_COLLECTION (keyring));
	}

	seahorse_keyserver_sync_show (keys, GTK_WINDOW (catalog));
	g_list_free (keys);
}

#endif /* WITH_KEYSERVER */

static void
on_pgp_generate_key (GSimpleAction *action,
                     GVariant *param,
                     gpointer user_data)
{
	SeahorseActionGroup *actions = SEAHORSE_ACTION_GROUP (user_data);
	SeahorseGpgmeKeyring* keyring;
	SeahorseCatalog *catalog;

	keyring = seahorse_pgp_backend_get_default_keyring (NULL);
	g_return_if_fail (keyring != NULL);

	catalog = seahorse_action_group_get_catalog (actions);
	seahorse_gpgme_generate_show (keyring,
                                  GTK_WINDOW (catalog),
	                              NULL, NULL, NULL);
	g_clear_object (&catalog);
}

static const GActionEntry ACTION_ENTRIES[] = {
    { "pgp-generate-key", on_pgp_generate_key },
#ifdef WITH_KEYSERVER
    { "remote-sync",      on_remote_sync },
    { "remote-find",      on_remote_find }
#endif /* WITH_KEYSERVER */
};

static void
seahorse_pgp_backend_actions_init (SeahorsePgpBackendActions *self)
{
    GActionMap *action_map = G_ACTION_MAP (self);
    GAction *generator_action = NULL;

    g_action_map_add_action_entries (action_map,
                                     ACTION_ENTRIES,
                                     G_N_ELEMENTS (ACTION_ENTRIES),
                                     self);

    /* Register generator actions */
    generator_action = g_action_map_lookup_action (action_map,
                                                   "pgp-generate-key");
    g_object_set_data (G_OBJECT (generator_action),
                       "label", _("PGP Key"));
    g_object_set_data (G_OBJECT (generator_action),
                       "description", _("Used to encrypt email and files"));
    seahorse_registry_register_object (G_OBJECT (generator_action),
                                       "generator");
}

static void
seahorse_pgp_backend_actions_class_init (SeahorsePgpBackendActionsClass *klass)
{

}

SeahorseActionGroup *
seahorse_pgp_backend_actions_instance (void)
{
	static SeahorseActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_PGP_TYPE_BACKEND_ACTIONS,
		                        "prefix", "pgp",
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
#define seahorse_gpgme_key_actions(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PGP_TYPE_ACTIONS, SeahorseGpgmeKeyActions))
#define seahorse_gpgme_key_actions_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PGP_TYPE_ACTIONS, SeahorseGpgmeKeyActionsClass))
#define SEAHORSE_PGP_IS_ACTIONS(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PGP_TYPE_ACTIONS))
#define SEAHORSE_PGP_IS_ACTIONS_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PGP_TYPE_ACTIONS))
#define seahorse_gpgme_key_actions_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PGP_TYPE_ACTIONS, SeahorseGpgmeKeyActionsClass))

typedef struct {
	SeahorseActionGroup parent_instance;
} SeahorseGpgmeKeyActions;

typedef struct {
	SeahorseActionGroupClass parent_class;
} SeahorseGpgmeKeyActionsClass;

G_DEFINE_TYPE (SeahorseGpgmeKeyActions, seahorse_gpgme_key_actions, SEAHORSE_TYPE_ACTION_GROUP);

static void
seahorse_gpgme_key_actions_init (SeahorseGpgmeKeyActions *self)
{
}

static void
seahorse_gpgme_key_actions_class_init (SeahorseGpgmeKeyActionsClass *klass)
{

}

SeahorseActionGroup *
seahorse_gpgme_key_actions_instance (void)
{
	static SeahorseActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_GPGME_KEY_ACTIONS,
		                        "prefix", "gpgme",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}
