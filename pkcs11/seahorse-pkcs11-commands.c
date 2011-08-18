/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-pkcs11-commands.h"

#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-certificate.h"
#include "seahorse-pkcs11-certificate-props.h"
#include "seahorse-pkcs11-operations.h"

#include "seahorse-progress.h"
#include "seahorse-util.h"

#include "common/seahorse-registry.h"

enum {
	PROP_0
};

struct _SeahorsePkcs11CommandsPrivate {
	GtkActionGroup *action_group;
};

static GQuark slot_certificate_window = 0; 

G_DEFINE_TYPE (SeahorsePkcs11Commands, seahorse_pkcs11_commands, SEAHORSE_TYPE_COMMANDS);

#define SEAHORSE_PKCS11_COMMANDS_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_TYPE_COMMANDS, SeahorsePkcs11CommandsPrivate))

static SeahorseObjectPredicate commands_predicate = { 0, };

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static void 
properties_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
seahorse_pkcs11_commands_show_properties (SeahorseCommands *cmds, SeahorseObject *object)
{
	GtkWindow *window;
	gpointer previous;
	
	g_return_if_fail (SEAHORSE_PKCS11_IS_COMMANDS (cmds));
	g_return_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (object));
	
	/* Try to show an already present window */
	previous = g_object_get_qdata (G_OBJECT (object), slot_certificate_window);
	if (GTK_IS_WINDOW (previous)) {
		window = GTK_WINDOW (previous);
		if (gtk_widget_get_visible (GTK_WIDGET (window))) {
			gtk_window_present (window);
			return;
		}
	}
	
	/* Create a new dialog for the certificate */
	window = GTK_WINDOW (seahorse_pkcs11_certificate_props_new (GCR_CERTIFICATE (object)));
	gtk_window_set_transient_for (window, seahorse_view_get_window (seahorse_commands_get_view (cmds)));
	g_object_set_qdata (G_OBJECT (object), slot_certificate_window, window);
	gtk_widget_show (GTK_WIDGET (window));

	/* Close the window when we get a response */
	g_signal_connect (window, "response", G_CALLBACK (properties_response), NULL);
}

static void
on_delete_completed (GObject *source,
                     GAsyncResult *result,
                     gpointer user_data)
{
	SeahorseCommands *self = SEAHORSE_COMMANDS (user_data);
	GError *error = NULL;

	if (!seahorse_pkcs11_delete_finish (result, &error))
		seahorse_util_handle_error (&error, seahorse_view_get_window (seahorse_commands_get_view (self)),
		                            _("Couldn't delete"));

	g_object_unref (self);
}

static gboolean
seahorse_pkcs11_commands_delete_objects (SeahorseCommands *cmds, GList *objects)
{
	GCancellable *cancellable;
	gchar *prompt;
	const gchar *display;
	GtkWidget *parent;
	gboolean ret;
	guint num;

	g_return_val_if_fail (SEAHORSE_PKCS11_IS_COMMANDS (cmds), FALSE);

	num = g_list_length (objects);
	
	if (num == 1) {
		display = seahorse_object_get_label (SEAHORSE_OBJECT (objects->data));
		prompt = g_strdup_printf (_("Are you sure you want to delete the certificate '%s'?"), display);
	} else {
		prompt = g_strdup_printf (ngettext (
				"Are you sure you want to delete %d certificate?",
				"Are you sure you want to delete %d certificates?",
				num), num);
	}

	parent = GTK_WIDGET (seahorse_view_get_window (seahorse_commands_get_view (cmds)));
	ret = seahorse_util_prompt_delete (prompt, parent);
	g_free (prompt);

	if (ret) {
		cancellable = g_cancellable_new ();
		seahorse_pkcs11_delete_async (objects, cancellable,
		                              on_delete_completed, g_object_ref (cmds));
		seahorse_progress_show (cancellable, _("Deleting"), TRUE);
		g_object_unref (cancellable);
	}

	return ret;
}

static GObject* 
seahorse_pkcs11_commands_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	GObject *obj = G_OBJECT_CLASS (seahorse_pkcs11_commands_parent_class)->constructor (type, n_props, props);
	SeahorsePkcs11CommandsPrivate *pv;
	SeahorsePkcs11Commands *self = NULL;
	SeahorseCommands *base;
	SeahorseView *view;
	
	if (obj) {
		pv = SEAHORSE_PKCS11_COMMANDS_GET_PRIVATE (obj);
		self = SEAHORSE_PKCS11_COMMANDS (obj);
		base = SEAHORSE_COMMANDS (self);
	
		view = seahorse_commands_get_view (base);
		g_return_val_if_fail (view, NULL);
		
		seahorse_view_register_commands (view, &commands_predicate, base);
		seahorse_view_register_ui (view, &commands_predicate, "", pv->action_group);
	}
	
	return obj;
}

static void
seahorse_pkcs11_commands_init (SeahorsePkcs11Commands *self)
{
	SeahorsePkcs11CommandsPrivate *pv = SEAHORSE_PKCS11_COMMANDS_GET_PRIVATE (self);
	pv->action_group = gtk_action_group_new ("pkcs11");
}

static void
seahorse_pkcs11_commands_dispose (GObject *obj)
{
	SeahorsePkcs11Commands *self = SEAHORSE_PKCS11_COMMANDS (obj);
	SeahorsePkcs11CommandsPrivate *pv = SEAHORSE_PKCS11_COMMANDS_GET_PRIVATE (self);
    
	if (pv->action_group)
		g_object_unref (pv->action_group);
	pv->action_group = NULL;
	
	G_OBJECT_CLASS (seahorse_pkcs11_commands_parent_class)->dispose (obj);
}

static void
seahorse_pkcs11_commands_finalize (GObject *obj)
{
	SeahorsePkcs11Commands *self = SEAHORSE_PKCS11_COMMANDS (obj);
	SeahorsePkcs11CommandsPrivate *pv = SEAHORSE_PKCS11_COMMANDS_GET_PRIVATE (self);
	
	g_assert (pv->action_group == NULL);

	G_OBJECT_CLASS (seahorse_pkcs11_commands_parent_class)->finalize (obj);
}

static void
seahorse_pkcs11_commands_set_property (GObject *obj, guint prop_id, const GValue *value, 
                           GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_commands_get_property (GObject *obj, guint prop_id, GValue *value, 
                                       GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
seahorse_pkcs11_commands_class_init (SeahorsePkcs11CommandsClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	SeahorseCommandsClass *cmd_class = SEAHORSE_COMMANDS_CLASS (klass);
	
	seahorse_pkcs11_commands_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11CommandsPrivate));

	gobject_class->constructor = seahorse_pkcs11_commands_constructor;
	gobject_class->dispose = seahorse_pkcs11_commands_dispose;
	gobject_class->finalize = seahorse_pkcs11_commands_finalize;
	gobject_class->set_property = seahorse_pkcs11_commands_set_property;
	gobject_class->get_property = seahorse_pkcs11_commands_get_property;
    
	cmd_class->show_properties = seahorse_pkcs11_commands_show_properties;
	cmd_class->delete_objects = seahorse_pkcs11_commands_delete_objects;

	slot_certificate_window = g_quark_from_static_string ("seahorse-pkcs11-commands-window");

	commands_predicate.type = SEAHORSE_PKCS11_TYPE_CERTIFICATE;
		
	/* Register this as a source of commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_PKCS11_TYPE_COMMANDS, 
	                                 SEAHORSE_PKCS11_TYPE_STR, "commands", NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePkcs11Commands*
seahorse_pkcs11_commands_new (void)
{
	return g_object_new (SEAHORSE_PKCS11_TYPE_COMMANDS, NULL);
}
