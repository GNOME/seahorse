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
#include "seahorse-pgp-commands.h"
#include "seahorse-pgp-dialogs.h"
#include "seahorse-keyserver-search.h"
#include "seahorse-keyserver-sync.h"

#include "seahorse-object.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

enum {
	PROP_0
};

struct _SeahorsePgpCommandsPrivate {
	GtkActionGroup* command_actions;
};

G_DEFINE_TYPE (SeahorsePgpCommands, seahorse_pgp_commands, SEAHORSE_TYPE_COMMANDS);

static const char* UI_DEFINITION = ""\
"<ui>"\
"	<menubar>"\
"		<menu name='File' action='file-menu'>"\
"			<placeholder name='FileCommands'>"\
"				<menuitem action='key-sign'/>"\
"			</placeholder>"\
"		</menu>"\
"		<placeholder name='RemoteMenu'>"\
"			<menu name='Remote' action='remote-menu'>"\
"				<menuitem action='remote-find'/>"\
"				<menuitem action='remote-sync'/>"\
"			</menu>"\
"		</placeholder>"\
"	</menubar>"\
"	<toolbar name='MainToolbar'>"\
"		<placeholder name='ToolItems'>"\
"			<toolitem action='key-sign'/>"\
"		</placeholder>"\
"	</toolbar>"\
"	<popup name='KeyPopup'>"\
"		<menuitem action='key-sign'/>"\
"	</popup>"\
"</ui>";

static SeahorsePredicate actions_key_predicate = { 0 };
static SeahorsePredicate actions_uid_predicate = { 0 };
static SeahorsePredicate commands_key_predicate = { 0 };
static SeahorsePredicate commands_uid_predicate = { 0 };

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void 
on_key_sign (GtkAction* action, SeahorsePgpCommands* self) 
{
	SeahorseView *view;
	GtkWindow *window;
	GList *keys;

	g_return_if_fail (SEAHORSE_IS_PGP_COMMANDS (self));
	g_return_if_fail (GTK_IS_ACTION (action));
	
	view = seahorse_commands_get_view (SEAHORSE_COMMANDS (self));
	keys = seahorse_view_get_selected_matching (view, &actions_key_predicate);

	if (keys == NULL) {
		keys = seahorse_view_get_selected_matching (view, &actions_uid_predicate);
		if (keys == NULL)
			return;
	}

	/* Indicate what we're actually going to operate on */
	seahorse_view_set_selected (view, keys->data);

	window = seahorse_commands_get_window (SEAHORSE_COMMANDS (self));
	
	if (G_TYPE_FROM_INSTANCE (keys->data) == SEAHORSE_TYPE_GPGME_KEY) {
		seahorse_gpgme_sign_prompt (SEAHORSE_GPGME_KEY (keys->data), window);
	} else if (G_TYPE_FROM_INSTANCE (keys->data) == SEAHORSE_TYPE_GPGME_UID) {
		seahorse_gpgme_sign_prompt_uid (SEAHORSE_GPGME_UID (keys->data), window);
	}
}

static void
on_remote_find (GtkAction* action,
                gpointer user_data)
{
	SeahorseCommands *commands = SEAHORSE_COMMANDS (user_data);
	seahorse_keyserver_search_show (seahorse_commands_get_window (commands));
}

static void
on_remote_sync (GtkAction* action,
                gpointer user_data)
{
	SeahorseCommands *commands = SEAHORSE_COMMANDS (user_data);
	SeahorseView *view = seahorse_commands_get_view (commands);
	SeahorseGpgmeKeyring *keyring;
	GList* objects;

	objects = seahorse_view_get_selected_objects (view);
	if (objects == NULL) {
		keyring = seahorse_pgp_backend_get_default_keyring (NULL);
		objects = gcr_collection_get_objects (GCR_COLLECTION (keyring));
	}

	seahorse_keyserver_sync_show (objects, seahorse_commands_get_window (commands));
	g_list_free (objects);
}

static const GtkActionEntry COMMAND_ENTRIES[] = {
	{ "key-sign", GTK_STOCK_INDEX, N_("_Sign Key..."), "",
	  N_("Sign public key"), G_CALLBACK (on_key_sign) },
	{ "remote-find", GTK_STOCK_FIND, N_("_Find Remote Keys..."), "",
	  N_("Search for keys on a key server"), G_CALLBACK (on_remote_find) },
	{ "remote-sync", GTK_STOCK_REFRESH, N_("_Sync and Publish Keys..."), "",
	  N_("Publish and/or synchronize your keys with those online."), G_CALLBACK (on_remote_sync) }
};


/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_pgp_commands_show_properties (SeahorseCommands* base, SeahorseObject* obj) 
{
	g_return_if_fail (SEAHORSE_IS_OBJECT (obj));

	if (SEAHORSE_IS_PGP_UID (obj))
		obj = SEAHORSE_OBJECT (seahorse_pgp_uid_get_parent (SEAHORSE_PGP_UID (obj)));

	g_return_if_fail (G_TYPE_FROM_INSTANCE (G_OBJECT (obj)) == SEAHORSE_TYPE_PGP_KEY || 
	                  G_TYPE_FROM_INSTANCE (G_OBJECT (obj)) == SEAHORSE_TYPE_GPGME_KEY);
	seahorse_pgp_key_properties_show (SEAHORSE_PGP_KEY (obj), seahorse_commands_get_window (base));
}

static gboolean
seahorse_pgp_commands_delete_objects (SeahorseCommands* base, GList* objects) 
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

	num = g_list_length (objects);
	if (num == 0)
		return TRUE;

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
		return TRUE;
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

	parent = GTK_WIDGET (seahorse_commands_get_window (base));
	if (!seahorse_util_prompt_delete (message, parent)) {
		g_free (message);
		return FALSE;
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
			return FALSE;
		}
	}

	return TRUE;
}

static GObject* 
seahorse_pgp_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_pgp_commands_parent_class)->constructor (type, n_props, props);
	SeahorsePgpCommands *self = NULL;
	SeahorseCommands *base;
	SeahorseView *view;
	
	if (obj) {
		self = SEAHORSE_PGP_COMMANDS (obj);
		base = SEAHORSE_COMMANDS (self);
	
		view = seahorse_commands_get_view (base);
		g_return_val_if_fail (view, NULL);
		
		self->pv->command_actions = gtk_action_group_new ("pgp");
		gtk_action_group_set_translation_domain (self->pv->command_actions, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (self->pv->command_actions, COMMAND_ENTRIES, 
		                              G_N_ELEMENTS (COMMAND_ENTRIES), self);

		seahorse_view_register_commands (view, &commands_key_predicate, base);
		seahorse_view_register_commands (view, &commands_uid_predicate, base);
		seahorse_view_register_ui (view, &actions_key_predicate, UI_DEFINITION, self->pv->command_actions);
	}
	
	return obj;
}

static void
seahorse_pgp_commands_init (SeahorsePgpCommands *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_PGP_COMMANDS, SeahorsePgpCommandsPrivate);

}

static void
seahorse_pgp_commands_dispose (GObject *obj)
{
	SeahorsePgpCommands *self = SEAHORSE_PGP_COMMANDS (obj);
    
	if (self->pv->command_actions)
		g_object_unref (self->pv->command_actions);
	self->pv->command_actions = NULL;
	
	G_OBJECT_CLASS (seahorse_pgp_commands_parent_class)->dispose (obj);
}

static void
seahorse_pgp_commands_finalize (GObject *obj)
{
	SeahorsePgpCommands *self = SEAHORSE_PGP_COMMANDS (obj);

	g_assert (!self->pv->command_actions);
	
	G_OBJECT_CLASS (seahorse_pgp_commands_parent_class)->finalize (obj);
}

static void
seahorse_pgp_commands_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pgp_commands_get_property (GObject *obj, guint prop_id, GValue *value, 
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pgp_commands_class_init (SeahorsePgpCommandsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_pgp_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePgpCommandsPrivate));

	gobject_class->constructor = seahorse_pgp_commands_constructor;
	gobject_class->dispose = seahorse_pgp_commands_dispose;
	gobject_class->finalize = seahorse_pgp_commands_finalize;
	gobject_class->set_property = seahorse_pgp_commands_set_property;
	gobject_class->get_property = seahorse_pgp_commands_get_property;
    
	SEAHORSE_COMMANDS_CLASS (klass)->show_properties = seahorse_pgp_commands_show_properties;
	SEAHORSE_COMMANDS_CLASS (klass)->delete_objects = seahorse_pgp_commands_delete_objects;
	
	/* Setup predicate for matching entries for these commands */
	commands_key_predicate.type = SEAHORSE_TYPE_PGP_KEY;
	commands_uid_predicate.type = SEAHORSE_TYPE_PGP_UID;
	actions_key_predicate.type = SEAHORSE_TYPE_PGP_KEY;
	actions_uid_predicate.type = SEAHORSE_TYPE_PGP_UID;

	/* Register this class as a commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_TYPE_PGP_COMMANDS, 
	                                 "commands", NULL, NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePgpCommands*
seahorse_pgp_commands_new (void)
{
	return g_object_new (SEAHORSE_TYPE_PGP_COMMANDS, NULL);
}
