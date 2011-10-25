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

#include "seahorse-ssh.h"
#include "seahorse-ssh-actions.h"
#include "seahorse-ssh-dialogs.h"
#include "seahorse-ssh-operation.h"

#include "seahorse-action.h"
#include "seahorse-actions.h"
#include "seahorse-object.h"
#include "seahorse-object-list.h"
#include "seahorse-registry.h"
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
"		<menu name='Remote' action='remote-menu'>"\
"			<menuitem action='remote-ssh-upload'/>"\
"		</menu>"\
"	</menubar>"\
"	<popup name='ObjectPopup'>"\
"		<menuitem action='remote-ssh-upload'/>"\
"	</popup>"\
"</ui>";

static void
on_ssh_upload (GtkAction* action,
               gpointer user_data)
{
	GList *ssh_keys = user_data;

	if (ssh_keys == NULL)
		return;

	seahorse_ssh_upload_prompt (ssh_keys, seahorse_action_get_window (action));
}

static void
on_show_properties (GtkAction *action,
                    gpointer user_data)
{
	seahorse_ssh_key_properties_show (SEAHORSE_SSH_KEY (user_data),
	                                  seahorse_action_get_window (action));
}

static void
on_delete_objects (GtkAction *action,
                   gpointer user_data)
{
	guint num;
	gchar* prompt;
	GList *l;
	GtkWidget *parent;
	GError *error = NULL;
	GList* objects;

	objects = user_data;
	num = g_list_length (objects);
	if (num == 0) {
		return;

	} else if (num == 1) {
		prompt = g_strdup_printf (_("Are you sure you want to delete the secure shell key '%s'?"),
		                          seahorse_object_get_label (objects->data));

	} else {
		prompt = g_strdup_printf (_("Are you sure you want to delete %d secure shell keys?"), num);
	}

	parent = GTK_WIDGET (seahorse_action_get_window (action));
	if (seahorse_util_prompt_delete (prompt, NULL)) {
		for (l = objects; l != NULL; l = g_list_next (l)) {
			if (!seahorse_ssh_op_delete_sync (l->data, &error)) {
				seahorse_util_handle_error (&error, parent, _("Couldn't delete key"));
			}
		}
	} else {
		g_cancellable_cancel (g_cancellable_get_current ());
	}

	g_free (prompt);

}

static const GtkActionEntry KEYS_ACTIONS[] = {
	{ "remote-ssh-upload", NULL, N_ ("Configure Key for _Secure Shell..."), "",
		N_ ("Send public Secure Shell key to another machine, and enable logins using that key."),
		G_CALLBACK (on_ssh_upload) },
	{ "delete", GTK_STOCK_DELETE, NULL, NULL,
	  N_("Delete the key."), G_CALLBACK (on_delete_objects) },
};

static const GtkActionEntry KEY_ACTIONS[] = {
	{ "properties", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Properties of the key."), G_CALLBACK (on_show_properties) },
};

static void
seahorse_ssh_actions_init (SeahorseSshActions *self)
{
	GtkActionGroup *actions = GTK_ACTION_GROUP (self);
	gtk_action_group_set_translation_domain (actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (actions, KEYS_ACTIONS, G_N_ELEMENTS (KEYS_ACTIONS), NULL);
	gtk_action_group_set_visible (actions, FALSE);
	seahorse_actions_register_definition (SEAHORSE_ACTIONS (self), UI_DEFINITION);
}

static GtkActionGroup *
seahorse_ssh_actions_clone_for_objects (SeahorseActions *actions,
                                        GList *objects)
{
	GtkActionGroup *cloned;

	g_return_val_if_fail (actions, NULL);

	cloned = gtk_action_group_new ("SshKey");
	gtk_action_group_set_translation_domain (cloned, GETTEXT_PACKAGE);

	gtk_action_group_add_actions_full (cloned, KEYS_ACTIONS,
	                                   G_N_ELEMENTS (KEYS_ACTIONS),
	                                   seahorse_object_list_copy (objects),
	                                   seahorse_object_list_free);

	/* A single object */
	if (!objects->next)
		gtk_action_group_add_actions_full (cloned, KEY_ACTIONS,
		                                   G_N_ELEMENTS (KEY_ACTIONS),
		                                   g_object_ref (objects->data),
		                                   g_object_unref);

	return cloned;
}

static void
seahorse_ssh_actions_class_init (SeahorseSshActionsClass *klass)
{
	SeahorseActionsClass *actions_class = SEAHORSE_ACTIONS_CLASS (klass);
	actions_class->clone_for_objects = seahorse_ssh_actions_clone_for_objects;
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
