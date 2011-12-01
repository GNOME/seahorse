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
#include "seahorse-interaction.h"
#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-properties.h"
#include "seahorse-token.h"

#include "seahorse-action.h"
#include "seahorse-actions.h"
#include "seahorse-delete-dialog.h"
#include "seahorse-object-list.h"
#include "seahorse-progress.h"
#include "seahorse-registry.h"
#include "seahorse-util.h"

GType   seahorse_pkcs11_token_actions_get_type       (void) G_GNUC_CONST;
#define SEAHORSE_TYPE_PKCS11_TOKEN_ACTIONS           (seahorse_pkcs11_token_actions_get_type ())
#define SEAHORSE_PKCS11_TOKEN_ACTIONS(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_TOKEN_ACTIONS, SeahorsePkcs11TokenActions))
#define SEAHORSE_PKCS11_IS_ACTIONS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_TOKEN_ACTIONS))

typedef struct {
	SeahorseActions parent;
} SeahorsePkcs11TokenActions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorsePkcs11TokenActionsClass;

G_DEFINE_TYPE (SeahorsePkcs11TokenActions, seahorse_pkcs11_token_actions, SEAHORSE_TYPE_ACTIONS);

static void
on_token_locked (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GError *error = NULL;

	seahorse_token_lock_finish (SEAHORSE_TOKEN (source), result, &error);
	if (error != NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("couldn't unlock token: %s", error->message);
		g_error_free (error);
	}
}

static void
on_token_lock (GtkAction *action,
               gpointer user_data)
{
	GTlsInteraction *interaction;

	interaction = seahorse_interaction_new (seahorse_action_get_window (action));
	seahorse_token_lock_async (SEAHORSE_TOKEN (user_data), interaction, NULL,
	                           on_token_locked, NULL);
	g_object_unref (interaction);
}


static void
on_token_unlocked (GObject *source,
                   GAsyncResult *result,
                   gpointer user_data)
{
	GError *error = NULL;

	seahorse_token_unlock_finish (SEAHORSE_TOKEN (source), result, &error);
	if (error != NULL) {
		if (!g_error_matches (error, GCK_ERROR, CKR_FUNCTION_CANCELED))
			g_warning ("couldn't unlock token: %s", error->message);
		g_error_free (error);
	}
}

static void
on_token_unlock (GtkAction *action,
                 gpointer user_data)
{
	GTlsInteraction *interaction;
	GtkWindow *window;

	window = seahorse_action_get_window (action);
	interaction = seahorse_interaction_new (window);

	seahorse_token_unlock_async (SEAHORSE_TOKEN (user_data), interaction, NULL,
	                             on_token_unlocked, NULL);

	g_object_unref (interaction);
}

static const GtkActionEntry TOKEN_ACTIONS[] = {
	{ "lock", NULL, NULL, NULL,
	  N_("Lock this token"), G_CALLBACK (on_token_lock) },
	{ "unlock", NULL, NULL, NULL,
	  N_("Unlock this token"), G_CALLBACK (on_token_unlock) },
};

static void
seahorse_pkcs11_token_actions_init (SeahorsePkcs11TokenActions *self)
{

}

static GtkActionGroup *
seahorse_pkcs11_token_actions_clone_for_objects (SeahorseActions *actions,
                                                 GList *objects)
{
	GtkActionGroup *cloned;

	g_return_val_if_fail (objects != NULL, NULL);

	cloned = gtk_action_group_new ("Pkcs11Token");

	if (!objects->next) {
		gtk_action_group_add_actions_full (cloned, TOKEN_ACTIONS,
		                                   G_N_ELEMENTS (TOKEN_ACTIONS),
		                                   g_object_ref (objects->data),
		                                   g_object_unref);

		g_object_bind_property (objects->data, "lockable",
		                        gtk_action_group_get_action (cloned, "lock"), "sensitive",
		                        G_BINDING_SYNC_CREATE);
		g_object_bind_property (objects->data, "unlockable",
		                        gtk_action_group_get_action (cloned, "unlock"), "sensitive",
		                        G_BINDING_SYNC_CREATE);
	}

	return cloned;
}

static void
seahorse_pkcs11_token_actions_class_init (SeahorsePkcs11TokenActionsClass *klass)
{
	SeahorseActionsClass *actions_class = SEAHORSE_ACTIONS_CLASS (klass);
	actions_class->clone_for_objects = seahorse_pkcs11_token_actions_clone_for_objects;
}

GtkActionGroup *
seahorse_pkcs11_token_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_PKCS11_TOKEN_ACTIONS,
		                        "name", "Pkcs11Token",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}

GType   seahorse_pkcs11_object_actions_get_type       (void) G_GNUC_CONST;
#define SEAHORSE_TYPE_PKCS11_OBJECT_ACTIONS           (seahorse_pkcs11_object_actions_get_type ())
#define SEAHORSE_PKCS11_OBJECT_ACTIONS(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_OBJECT_ACTIONS, SeahorsePkcs11ObjectActions))
#define SEAHORSE_PKCS11_IS_OBJECT_ACTIONS(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_OBJECT_ACTIONS))

typedef struct {
	SeahorseActions parent;
} SeahorsePkcs11ObjectActions;

typedef struct {
	SeahorseActionsClass parent_class;
} SeahorsePkcs11ObjectActionsClass;

G_DEFINE_TYPE (SeahorsePkcs11ObjectActions, seahorse_pkcs11_object_actions, SEAHORSE_TYPE_ACTIONS);

static void
on_show_properties (GtkAction *action,
                    gpointer user_data)
{
	GtkWindow *window;
	GObject *object;

	/* Create a new dialog for the certificate */
	object = G_OBJECT (user_data);
	window = seahorse_pkcs11_properties_show (object, seahorse_action_get_window (action));
	gtk_widget_show (GTK_WIDGET (window));
}


static const GtkActionEntry CERTIFICATE_ACTIONS[] = {
	{ "properties", GTK_STOCK_PROPERTIES, NULL, NULL,
	  N_("Properties of the certificate."), G_CALLBACK (on_show_properties) },
};

static void
seahorse_pkcs11_object_actions_init (SeahorsePkcs11ObjectActions *self)
{

}

static GtkActionGroup *
seahorse_pkcs11_object_actions_clone_for_objects (SeahorseActions *actions,
                                           GList *objects)
{
	GtkActionGroup *cloned;

	g_return_val_if_fail (objects != NULL, NULL);

	cloned = gtk_action_group_new ("Pkcs11Object");
	gtk_action_group_set_translation_domain (cloned, GETTEXT_PACKAGE);

	/* Only one object? */
	if (!objects->next)
		gtk_action_group_add_actions_full (cloned, CERTIFICATE_ACTIONS,
		                                   G_N_ELEMENTS (CERTIFICATE_ACTIONS),
		                                   g_object_ref (objects->data),
		                                   g_object_unref);

	return cloned;
}

static void
seahorse_pkcs11_object_actions_class_init (SeahorsePkcs11ObjectActionsClass *klass)
{
	SeahorseActionsClass *actions_class = SEAHORSE_ACTIONS_CLASS (klass);
	actions_class->clone_for_objects = seahorse_pkcs11_object_actions_clone_for_objects;
}

GtkActionGroup *
seahorse_pkcs11_object_actions_instance (void)
{
	static GtkActionGroup *actions = NULL;

	if (actions == NULL) {
		actions = g_object_new (SEAHORSE_TYPE_PKCS11_OBJECT_ACTIONS,
		                        "name", "Pkcs11Object",
		                        NULL);
		g_object_add_weak_pointer (G_OBJECT (actions),
		                           (gpointer *)&actions);
	} else {
		g_object_ref (actions);
	}

	return actions;
}
