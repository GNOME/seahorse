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

#include "seahorse-interaction.h"
#include "seahorse-passphrase.h"

#include <glib/gi18n.h>
#include <gcr/gcr.h>

enum {
	PROP_0,
	PROP_PARENT
};

struct _SeahorseInteractionPrivate {
	GtkWindow *parent;
};

G_DEFINE_TYPE (SeahorseInteraction, seahorse_interaction, G_TYPE_TLS_INTERACTION);

static void
seahorse_interaction_init (SeahorseInteraction *self)
{
	self->pv = (G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_INTERACTION,
	                                         SeahorseInteractionPrivate));
}

static void
seahorse_interaction_set_property (GObject *obj,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	SeahorseInteraction *self = SEAHORSE_INTERACTION (obj);

	switch (prop_id) {
	case PROP_PARENT:
		seahorse_interaction_set_parent (self, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_interaction_get_property (GObject *obj,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	SeahorseInteraction *self = SEAHORSE_INTERACTION (obj);

	switch (prop_id) {
	case PROP_PARENT:
		g_value_set_object (value, seahorse_interaction_get_parent (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_interaction_dispose (GObject *obj)
{
	SeahorseInteraction *self = SEAHORSE_INTERACTION (obj);

	seahorse_interaction_set_parent (self, NULL);

	G_OBJECT_CLASS (seahorse_interaction_parent_class)->dispose (obj);
}

static gchar *
calc_description (GTlsPassword *password)
{
	const gchar *description = g_tls_password_get_description (password);
	return g_strdup_printf (_("Enter PIN or password for: %s"), description);
}

static GTlsInteractionResult
seahorse_interaction_ask_password (GTlsInteraction *interaction,
                                   GTlsPassword *password,
                                   GCancellable *cancellable,
                                   GError **error)
{
	SeahorseInteraction *self = SEAHORSE_INTERACTION (interaction);
	GTlsInteractionResult res;
	GtkDialog *dialog;
	gchar *description;
	const gchar *pass;
	gsize length;

	description = calc_description (password);

	dialog = seahorse_passphrase_prompt_show (NULL, description, NULL, NULL, FALSE);

	g_free (description);

	if (self->pv->parent)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), self->pv->parent);

	switch (gtk_dialog_run (dialog)) {
	case GTK_RESPONSE_ACCEPT:
		pass = seahorse_passphrase_prompt_get (dialog);
		length = strlen (pass);
		g_tls_password_set_value_full (password,
		                               (guchar *)gcr_secure_memory_strdup (pass),
		                               length, gcr_secure_memory_free);
		res = G_TLS_INTERACTION_HANDLED;
		break;
	default:
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
		                     "The password request was cancelled by the user");
		res = G_TLS_INTERACTION_FAILED;
		break;
	};

	gtk_widget_destroy (GTK_WIDGET (dialog));
	return res;
}

static void
seahorse_interaction_class_init (SeahorseInteractionClass *klass)
{
	GTlsInteractionClass *interaction_class = G_TLS_INTERACTION_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = seahorse_interaction_get_property;
	object_class->set_property = seahorse_interaction_set_property;
	object_class->dispose = seahorse_interaction_dispose;

	interaction_class->ask_password = seahorse_interaction_ask_password;

	g_object_class_install_property (object_class, PROP_PARENT,
	            g_param_spec_object ("parent", "Parent", "Parent window",
	                                 GTK_TYPE_WINDOW, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (klass, sizeof (SeahorseInteractionPrivate));
}

GTlsInteraction *
seahorse_interaction_new (GtkWindow *parent)
{
	return g_object_new (SEAHORSE_TYPE_INTERACTION,
	                     "parent", parent,
	                     NULL);
}

GtkWindow *
seahorse_interaction_get_parent (SeahorseInteraction *self)
{
	g_return_val_if_fail (SEAHORSE_IS_INTERACTION (self), NULL);
	return self->pv->parent;
}

void
seahorse_interaction_set_parent (SeahorseInteraction *self,
                                 GtkWindow *parent)
{
	g_return_if_fail (SEAHORSE_IS_INTERACTION (self));
	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

	if (self->pv->parent)
		g_object_remove_weak_pointer (G_OBJECT (self->pv->parent),
		                              (gpointer *)&self->pv->parent);
	self->pv->parent = parent;
	if (self->pv->parent)
		g_object_add_weak_pointer (G_OBJECT (self->pv->parent),
		                           (gpointer *)&self->pv->parent);
	g_object_notify (G_OBJECT (self), "parent");
}
