/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#include "config.h"

#include "seahorse-ssh.h"
#include "seahorse-ssh-actions.h"
#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-common.h"
#include "seahorse-object.h"
#include "seahorse-object-list.h"
#include "seahorse-util.h"

#include <glib/gi18n.h>

#include <glib.h>
#include <glib-object.h>

GType   seahorse_ssh_actions_get_type             (void);
#define SEAHORSE_TYPE_SSH_ACTIONS                 (seahorse_ssh_actions_get_type ())
#define SEAHORSE_SSH_ACTIONS(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SSH_ACTIONS, SeahorseSshActions))
#define SEAHORSE_SSH_ACTIONS_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SSH_ACTIONS, SeahorseSshActionsClass))
#define SEAHORSE_IS_SSH_ACTIONS(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SSH_ACTIONS))
#define SEAHORSE_IS_SSH_ACTIONS_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SSH_ACTIONS))
#define SEAHORSE_SSH_ACTIONS_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SSH_ACTIONS, SeahorseSshActionsClass))

typedef struct {
	SeahorseActions parent_instance;
} SeahorseSshActions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorseSshActionsClass;

G_DEFINE_TYPE (SeahorseSshActions, seahorse_ssh_actions, SEAHORSE_TYPE_ACTIONS);

static const char* UI_DEFINITION = ""\
"<ui>"\
"	<menubar>"\
"		<placeholder name='RemoteMenu'>"\
"			<menu name='Remote' action='remote-menu'>"\
"				<menuitem action='remote-ssh-upload'/>"\
"			</menu>"\
"		</placeholder>"\
"	</menubar>"\
"	<popup name='ObjectPopup'>"\
"		<menuitem action='remote-ssh-upload'/>"\
"	</popup>"\
"</ui>";

static void
on_ssh_upload (GtkAction* action,
               gpointer user_data)
{
	SeahorseActions *actions = SEAHORSE_ACTIONS (user_data);
	SeahorseCatalog *catalog;
	GList *keys, *objects, *l;

	keys = NULL;
	catalog = seahorse_actions_get_catalog (actions);

	if (catalog != NULL) {
		objects = seahorse_catalog_get_selected_objects (catalog);
		for (l = objects; l != NULL; l = g_list_next (l)) {
			if (SEAHORSE_IS_SSH_KEY (l->data))
				keys = g_list_prepend (keys, l->data);
		}
		g_list_free (objects);
	}
	g_object_unref (catalog);

	seahorse_ssh_upload_prompt (keys, seahorse_action_get_window (action));
	g_list_free (keys);
}

static const GtkActionEntry KEYS_ACTIONS[] = {
	{ "remote-ssh-upload", NULL, N_ ("Configure Key for _Secure Shell..."), "",
		N_ ("Send public Secure Shell key to another machine, and enable logins using that key."),
		G_CALLBACK (on_ssh_upload) },
};

static void
seahorse_ssh_actions_init (SeahorseSshActions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, KEYS_ACTIONS, G_N_ELEMENTS (KEYS_ACTIONS), self);
	seahorse_actions_register_definition (SEAHORSE_ACTIONS (self), UI_DEFINITION);
}

static void
seahorse_ssh_actions_class_init (SeahorseSshActionsClass *klass)
{

}

GtkActionGroup *
seahorse_ssh_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_SSH_ACTIONS,
		                        "name", "SshKey",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}
