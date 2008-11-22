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

#include "seahorse-util.h"

#include "common/seahorse-registry.h"

#include "libcryptui/crui-x509-cert-dialog.h"

enum {
	PROP_0,
	PROP_KTYPE,
	PROP_UI_DEFINITION,
	PROP_COMMAND_ACTIONS
};

struct _SeahorsePkcs11CommandsPrivate {
	GtkActionGroup *action_group;
};

static GQuark slot_certificate_window = 0; 

G_DEFINE_TYPE (SeahorsePkcs11Commands, seahorse_pkcs11_commands, SEAHORSE_TYPE_COMMANDS);

#define SEAHORSE_PKCS11_COMMANDS_GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_TYPE_COMMANDS, SeahorsePkcs11CommandsPrivate))

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
		if (GTK_WIDGET_VISIBLE (window)) {
			gtk_window_present (window);
			return;
		}
	}
	
	/* Create a new dialog for the certificate */
	window = GTK_WINDOW (crui_x509_cert_dialog_new (CRUI_X509_CERT (object)));
	gtk_window_set_transient_for (window, seahorse_view_get_window (seahorse_commands_get_view (cmds)));
	g_object_set_qdata (G_OBJECT (object), slot_certificate_window, window);
	gtk_widget_show (GTK_WIDGET (window));

	/* Close the window when we get a response */
	g_signal_connect (window, "response", G_CALLBACK (properties_response), NULL);
}

static SeahorseOperation*
seahorse_pkcs11_commands_delete_objects (SeahorseCommands *cmds, GList *objects)
{
	gchar *prompt;
	gchar *display;
	gboolean ret;
	guint num;
	
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_COMMANDS (cmds), NULL);

	num = g_list_length (objects);
	
	if (num == 1) {
		display = seahorse_object_get_display_name (SEAHORSE_OBJECT (objects->data));
		prompt = g_strdup_printf (_("Are you sure you want to delete the certificate '%s'?"), display);
		g_free (display);
	} else {
		prompt = g_strdup_printf (_("Are you sure you want to delete %d secure shell keys?"), num);
	}
	
	ret = seahorse_util_prompt_delete (prompt, GTK_WIDGET (seahorse_view_get_window (seahorse_commands_get_view (cmds))));
	g_free (prompt);
	
	if (ret)
		return seahorse_source_delete_objects (objects);
	else
		return NULL;
}

static GQuark 
seahorse_pkcs11_commands_get_ktype (SeahorseCommands *base)
{
	return SEAHORSE_PKCS11_TYPE;
}

static const char* 
seahorse_pkcs11_commands_get_ui_definition (SeahorseCommands *base)
{
	return "";
}

static GtkActionGroup* 
seahorse_pkcs11_commands_get_command_actions (SeahorseCommands *base)
{
	SeahorsePkcs11Commands *self = SEAHORSE_PKCS11_COMMANDS (base);
	SeahorsePkcs11CommandsPrivate *pv = SEAHORSE_PKCS11_COMMANDS_GET_PRIVATE (self);
	return pv->action_group;
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
	SeahorseCommands *base = SEAHORSE_COMMANDS (obj);
	
	switch (prop_id) {
	case PROP_KTYPE:
		g_value_set_uint (value, seahorse_pkcs11_commands_get_ktype (base));
		break;
	case PROP_UI_DEFINITION:
		g_value_set_string (value, seahorse_pkcs11_commands_get_ui_definition (base));
		break;
	case PROP_COMMAND_ACTIONS:
		g_value_set_object (value, seahorse_pkcs11_commands_get_command_actions (base));
		break;
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

	gobject_class->dispose = seahorse_pkcs11_commands_dispose;
	gobject_class->finalize = seahorse_pkcs11_commands_finalize;
	gobject_class->set_property = seahorse_pkcs11_commands_set_property;
	gobject_class->get_property = seahorse_pkcs11_commands_get_property;
    
	cmd_class->show_properties = seahorse_pkcs11_commands_show_properties;
	cmd_class->delete_objects = seahorse_pkcs11_commands_delete_objects;
	cmd_class->get_ktype = seahorse_pkcs11_commands_get_ktype;
	cmd_class->get_ui_definition = seahorse_pkcs11_commands_get_ui_definition;
	cmd_class->get_command_actions = seahorse_pkcs11_commands_get_command_actions;

	g_object_class_override_property (gobject_class, PROP_KTYPE, "ktype");
	g_object_class_override_property (gobject_class, PROP_UI_DEFINITION, "ui-definition");
	g_object_class_override_property (gobject_class, PROP_COMMAND_ACTIONS, "command-actions");

	slot_certificate_window = g_quark_from_static_string ("seahorse-pkcs11-commands-window");

	/* Register this as a source of commands */
	seahorse_registry_register_type (seahorse_registry_get (), SEAHORSE_PKCS11_TYPE_COMMANDS, "commands", NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorsePkcs11Commands*
seahorse_pkcs11_commands_new (void)
{
	return g_object_new (SEAHORSE_PKCS11_TYPE_COMMANDS, NULL);
}
