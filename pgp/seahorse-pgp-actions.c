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
#include "seahorse-gpgme-generate-dialog.h"
#include "seahorse-gpgme-key.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-uid.h"
#include "seahorse-pgp-backend.h"
#include "seahorse-pgp-actions.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-keyserver-search.h"
#include "seahorse-keyserver-sync.h"

#include "seahorse-common.h"

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
    g_autoptr(SeahorseCatalog) catalog = NULL;
    SeahorseKeyserverSearch *search_dialog = NULL;

    /* Make a new "Find remote keys" dialog */
    catalog = seahorse_action_group_get_catalog (actions);
    search_dialog = seahorse_keyserver_search_new ();

    /* Run it and get the search text */
    adw_dialog_present (ADW_DIALOG (search_dialog), GTK_WIDGET (catalog));
}

static void
on_remote_sync (GSimpleAction *action,
                GVariant      *param,
                void          *user_data)
{
    SeahorseActionGroup *actions = SEAHORSE_ACTION_GROUP (user_data);
    g_autoptr(SeahorseCatalog) catalog = NULL;
    g_autoptr(GListModel) keys = NULL;
    SeahorseKeyserverSync *dialog = NULL;

    catalog = seahorse_action_group_get_catalog (actions);
    if (catalog != NULL) {
        g_autoptr(GList) items = NULL;
        g_autoptr(GListStore) store = NULL;

        store = g_list_store_new (SEAHORSE_PGP_TYPE_KEY);
        items = seahorse_catalog_get_selected_items (catalog);
        for (GList *l = items; l != NULL; l = g_list_next (l)) {
            if (SEAHORSE_PGP_IS_KEY (l->data))
                g_list_store_append (store, l->data);
        }
        keys = G_LIST_MODEL (g_steal_pointer (&store));
    }

    if (keys == NULL || g_list_model_get_n_items (G_LIST_MODEL (keys)) == 0) {
        SeahorseGpgmeKeyring *keyring;

        g_clear_object (&keys);
        keyring = seahorse_pgp_backend_get_default_keyring (NULL);
        keys = g_object_ref (G_LIST_MODEL (keyring));
    }

    dialog = seahorse_keyserver_sync_new (G_LIST_MODEL (keys));
    adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (catalog));
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
    SeahorseGpgmeGenerateDialog *dialog;

    keyring = seahorse_pgp_backend_get_default_keyring (NULL);
    g_return_if_fail (keyring != NULL);

    catalog = seahorse_action_group_get_catalog (actions);

    dialog = seahorse_gpgme_generate_dialog_new (keyring);
    adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (catalog));

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

    g_action_map_add_action_entries (action_map,
                                     ACTION_ENTRIES,
                                     G_N_ELEMENTS (ACTION_ENTRIES),
                                     self);
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
