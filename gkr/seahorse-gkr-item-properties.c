/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>

#include <glib/gi18n.h>

#include "seahorse-gkr-item.h"
#include "seahorse-gkr-source.h"
#include "seahorse-gkr-operation.h"

#include "seahorse-gtkstock.h"
#include "seahorse-object.h"
#include "seahorse-object-widget.h"
#include "seahorse-progress.h"
#include "seahorse-secure-memory.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"

#include "common/seahorse-bind.h"
#include "common/seahorse-secure-buffer.h"

GType
boxed_access_control_type (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("GnomeKeyringAccessControl", 
		                                     (GBoxedCopyFunc)gnome_keyring_access_control_copy,
		                                     (GBoxedFreeFunc)gnome_keyring_access_control_free);
	return type;
}

/* -----------------------------------------------------------------------------
 * MAIN TAB 
 */

static gboolean
transform_item_use (const GValue *from, GValue *to)
{
	gchar *label;
	guint val;
	
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_UINT, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	
	val = g_value_get_uint (from);
	switch (val) {
	case SEAHORSE_GKR_USE_NETWORK:
		label = _("Access a network share or resource");
		break;
	case SEAHORSE_GKR_USE_WEB:
		label = _("Access a website");
		break;
	case SEAHORSE_GKR_USE_PGP:
		label = _("Unlocks a PGP key");
		break;
	case SEAHORSE_GKR_USE_SSH:
		label = _("Unlocks a Secure Shell key");
		break;
	case SEAHORSE_GKR_USE_OTHER:
		label = _("Saved password or login");
		break;
	default:
		label = "";
	        g_return_val_if_reached (FALSE);
	};
	
	g_value_set_string (to, label);
	return TRUE;
}

static gboolean
transform_item_type (const GValue *from, GValue *to)
{
	gchar *label;
	guint val;
	
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_UINT, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	
	val = g_value_get_uint (from);
	switch (val) {
	case SEAHORSE_GKR_USE_NETWORK:
	case SEAHORSE_GKR_USE_WEB:
		label = _("Network Credentials");
		break;
	case SEAHORSE_GKR_USE_PGP:
	case SEAHORSE_GKR_USE_SSH:
	case SEAHORSE_GKR_USE_OTHER:
		label = _("Password");
		break;
	default:
		label = "";
	        g_return_val_if_reached (FALSE);
	};
	
	g_value_set_string (to, label);
	return TRUE;
}

static gboolean
transform_network_visible (const GValue *from, GValue *to)
{
	gboolean result;
	guint val;
	
	g_return_val_if_fail (G_VALUE_TYPE (from) == G_TYPE_UINT, FALSE);
	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_BOOLEAN, FALSE);
	
	val = g_value_get_uint (from);
	switch (val) {
	case SEAHORSE_GKR_USE_NETWORK:
	case SEAHORSE_GKR_USE_WEB:
		result = TRUE;
		break;
	case SEAHORSE_GKR_USE_PGP:
	case SEAHORSE_GKR_USE_SSH:
	case SEAHORSE_GKR_USE_OTHER:
		result = FALSE;
		break;
	default:
	        g_return_val_if_reached (FALSE);
	};
	
	g_value_set_boolean (to, result);
	return TRUE;
}

static gboolean
transform_attributes_server (const GValue *from, GValue *to)
{
	GnomeKeyringAttributeList *attrs;
	attrs = g_value_get_boxed (from);
	if (attrs)
		g_value_set_string (to, seahorse_gkr_find_string_attribute (attrs, "server"));
	if (!g_value_get_string (to))
		g_value_set_string (to, "");
	return TRUE;
}

static gboolean
transform_attributes_user (const GValue *from, GValue *to)
{
	GnomeKeyringAttributeList *attrs;
	attrs = g_value_get_boxed (from);
	if (attrs)
		g_value_set_string (to, seahorse_gkr_find_string_attribute (attrs, "user"));
	if (!g_value_get_string (to))
		g_value_set_string (to, "");
	return TRUE;
}

static void
transfer_password (SeahorseGkrItem *git, SeahorseWidget *swidget)
{
	GtkWidget *expander;
	GtkEntry *entry;
	const gchar *secret;

	expander = seahorse_widget_get_widget (swidget, "password-expander");
	g_return_if_fail (expander);

	entry = g_object_get_data (G_OBJECT (swidget), "secure-password-entry");
	g_return_if_fail (entry);

	if (gtk_expander_get_expanded (GTK_EXPANDER (expander))) {
		secret = seahorse_gkr_item_get_secret (git);
		gtk_entry_set_text (entry, secret ? secret : "");
	} else {
		gtk_entry_set_text (entry, "");
	}
	g_object_set_data (G_OBJECT (entry), "changed", NULL);
}

static void
on_update_password_ready (GObject *source,
                          GAsyncResult *result,
                          gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	SeahorseGkrItem *git = SEAHORSE_GKR_ITEM (source);
	GError *error = NULL;
	GtkWidget *expander;

	if (seahorse_gkr_update_secret_finish (git, result, &error)) {
		transfer_password (git, swidget);

	} else {
		seahorse_util_show_error (seahorse_widget_get_toplevel (swidget),
		                          _("Couldn't change password."), error->message);
		g_clear_error (&error);
	}

	expander = seahorse_widget_get_widget (swidget, "password-expander");
	gtk_widget_set_sensitive (expander, TRUE);
	g_object_set_data (G_OBJECT (swidget), "updating-password", NULL);

	g_object_unref (swidget);
}

static void
password_activate (GtkEntry *entry,
                   SeahorseWidget *swidget)
{
	SeahorseObject *object;
	SeahorseGkrItem *git;
	GtkWidget *expander;
	GCancellable *cancellable;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	if (!object)
		return;

	git = SEAHORSE_GKR_ITEM (object);

	expander = seahorse_widget_get_widget (swidget, "password-expander");
	g_return_if_fail (expander);

	if (!gtk_expander_get_expanded (GTK_EXPANDER (expander)))
		return;

	entry = g_object_get_data (G_OBJECT (swidget), "secure-password-entry");
	if (!g_object_get_data (G_OBJECT (entry), "changed"))
		return;

	if (g_object_get_data (G_OBJECT (swidget), "updating-password"))
		return;
	g_object_set_data (G_OBJECT (swidget), "updating-password", "updating");

	gtk_widget_set_sensitive (expander, FALSE);

	cancellable = g_cancellable_new ();
	seahorse_gkr_update_secret_async (git, gtk_entry_get_text (entry), cancellable,
	                                  on_update_password_ready, g_object_ref (swidget));
	seahorse_progress_show (cancellable, _("Updating password"), TRUE);
	g_object_unref (cancellable);
}

static void
password_changed (GtkEditable *editable, SeahorseWidget *swidget)
{
	g_object_set_data (G_OBJECT (editable), "changed", "changed");
}

static gboolean
password_focus_out (GtkEntry* entry, GdkEventFocus *event, SeahorseWidget *swidget)
{
    password_activate (entry, swidget);
    return FALSE;
}

G_MODULE_EXPORT void 
on_item_show_password_toggled (GtkToggleButton *button, SeahorseWidget *swidget)
{
	GtkWidget *widget = g_object_get_data (G_OBJECT (swidget), "secure-password-entry");
	gtk_entry_set_visibility (GTK_ENTRY (widget), gtk_toggle_button_get_active (button));
}

G_MODULE_EXPORT void
on_item_password_expander_activate (GtkExpander *expander, SeahorseWidget *swidget)
{
    SeahorseObject *object;
    SeahorseGkrItem *git;
    GtkWidget *widget;

    object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    if (!object)
	    return;
    git = SEAHORSE_GKR_ITEM (object);

    if (!gtk_expander_get_expanded (expander))
        return;

    /* Always have a hidden password when opening box */
    widget = seahorse_widget_get_widget (swidget, "show-password-check");
    g_return_if_fail (widget != NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
    
    /* Make sure to trigger retrieving the secret */
    transfer_password (git, swidget);
}

typedef struct {
	SeahorseWidget *swidget;
	GtkEntry *entry;
} item_description_closure;

static void
item_description_free (gpointer data)
{
	item_description_closure *closure = data;
	g_object_unref (closure->swidget);
	g_free (closure);
}

static void
on_item_description_complete (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
	item_description_closure *closure = user_data;
	SeahorseGkrItem *git = SEAHORSE_GKR_ITEM (source);
	GError *error = NULL;

	if (!seahorse_gkr_update_description_finish (git, result, &error)) {

		/* Set back to original */
		gtk_entry_set_text (closure->entry,
		                    seahorse_object_get_label (SEAHORSE_OBJECT (git)));

		seahorse_util_show_error (seahorse_widget_get_toplevel (closure->swidget),
		                          _("Couldn't set description."), error->message);
		g_clear_error (&error);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (closure->entry), TRUE);
	g_object_set_data (G_OBJECT (closure->swidget), "updating-description", NULL);

	item_description_free (closure);
}

G_MODULE_EXPORT void
on_item_description_activate (GtkWidget *entry,
                              SeahorseWidget *swidget)
{
	item_description_closure *closure;
	GCancellable *cancellable;
	SeahorseObject *object;
	SeahorseGkrItem *git;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
	if (!object)
		return;

	if (g_object_get_data (G_OBJECT (swidget), "updating-description"))
		return;
	g_object_set_data (G_OBJECT (swidget), "updating-description", "updating");

	git = SEAHORSE_GKR_ITEM (object);

	gtk_widget_set_sensitive (entry, FALSE);

	closure = g_new0 (item_description_closure, 1);
	closure->swidget = g_object_ref (swidget);
	closure->entry = GTK_ENTRY (entry);

	cancellable = g_cancellable_new ();
	seahorse_gkr_update_description_async (git, gtk_entry_get_text (GTK_ENTRY (entry)),
	                                       cancellable, on_item_description_complete,
	                                       closure);
	seahorse_progress_show (cancellable, _("Updating password"), TRUE);
	g_object_unref (cancellable);
}

G_MODULE_EXPORT gboolean
on_item_description_focus_out (GtkWidget* widget, GdkEventFocus *event, SeahorseWidget *swidget)
{
	on_item_description_activate (widget, swidget);
	return FALSE;
}


static void
setup_main (SeahorseWidget *swidget)
{
	SeahorseObject *object;
	GtkEntryBuffer *buffer;
	GtkWidget *widget;
	GtkWidget *box;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;

	/* Setup the image properly */
	seahorse_bind_property ("icon", object, "stock", 
	                        seahorse_widget_get_widget (swidget, "key-image"));
	
	/* Setup the label properly */
	seahorse_bind_property ("label", object, "text", 
	                        seahorse_widget_get_widget (swidget, "description-field") );
	
	/* Window title */
	seahorse_bind_property ("label", object, "title", 
	                        seahorse_widget_get_toplevel (swidget));
	
	/* Usage */
	seahorse_bind_property_full ("use", object, transform_item_use, "label", 
	                             seahorse_widget_get_widget (swidget, "use-field"), NULL);
	
	/* Item Type */
	seahorse_bind_property_full ("use", object, transform_item_type, "label", 
	                             seahorse_widget_get_widget (swidget, "type-field"), NULL);
	
	/* Network field visibility */
	seahorse_bind_property_full ("use", object, transform_network_visible, "visible",
	                             seahorse_widget_get_widget (swidget, "server-label"),
	                             seahorse_widget_get_widget (swidget, "server-field"),
	                             seahorse_widget_get_widget (swidget, "login-label"),
	                             seahorse_widget_get_widget (swidget, "login-field"), NULL);

	/* Server name */
	seahorse_bind_property_full ("item-attributes", object, transform_attributes_server, "label", 
	                             seahorse_widget_get_widget (swidget, "server-field"), NULL);
	
	/* User name */
	seahorse_bind_property_full ("item-attributes", object, transform_attributes_user, "label", 
	                             seahorse_widget_get_widget (swidget, "login-field"), NULL);

	/* Create the password entry */
	buffer = seahorse_secure_buffer_new ();
	widget = gtk_entry_new_with_buffer (buffer);
	g_object_unref (buffer);

	box = seahorse_widget_get_widget (swidget, "password-box-area");
	g_return_if_fail (box != NULL);
	gtk_container_add (GTK_CONTAINER (box), widget);
	g_object_set_data (G_OBJECT (swidget), "secure-password-entry", widget);
	gtk_widget_show (widget);
	        
	/* Now watch for changes in the password */
	g_signal_connect (widget, "activate", G_CALLBACK (password_activate), swidget);
	g_signal_connect (widget, "changed", G_CALLBACK (password_changed), swidget);
	g_signal_connect_after (widget, "focus-out-event", G_CALLBACK (password_focus_out), swidget);

	/* Sensitivity of the password entry */
	seahorse_bind_property ("has-secret", object, "sensitive", widget);

	/* Updating of the password entry */
	seahorse_bind_objects ("has-secret", object, (SeahorseTransfer)transfer_password, swidget);

	widget = seahorse_widget_get_widget (swidget, "show-password-check");
	on_item_show_password_toggled (GTK_TOGGLE_BUTTON (widget), swidget);
}

/* -----------------------------------------------------------------------------
 * DETAILS TAB
 */

static gboolean
transform_item_details (const GValue *from, GValue *to)
{
	GnomeKeyringAttributeList *attrs;
	GnomeKeyringAttribute *attr;
	GString *details;
	gchar *text;
	guint i;

	g_return_val_if_fail (G_VALUE_TYPE (to) == G_TYPE_STRING, FALSE);
	attrs = g_value_get_boxed (from);
	
	details = g_string_new (NULL);
	if (attrs) {
		/* Build up the display string */
		for(i = 0; i < attrs->len; ++i) {
			attr = &(gnome_keyring_attribute_list_index (attrs, i));
			text = g_markup_escape_text (attr->name, -1);
			g_string_append_printf (details, "<b>%s</b>: ", text);
			g_free (text);
			switch (attr->type) {
			case GNOME_KEYRING_ATTRIBUTE_TYPE_STRING:
				text = g_markup_escape_text (attr->value.string, -1);
				g_string_append_printf (details, "%s\n", text);
				g_free (text);
				break;
			case GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32:
				g_string_append_printf (details, "%u\n", attr->value.integer);
				break;
			default:
				g_string_append (details, "<i>[invalid]</i>\n");
				break;
			}
		}
	}

	g_value_take_string (to, g_string_free (details, FALSE));
	return TRUE;
}

static void 
setup_details (SeahorseWidget *swidget)
{
	SeahorseObject *object;
	GtkWidget *widget;

	object = SEAHORSE_OBJECT_WIDGET (swidget)->object;
    
	widget = seahorse_widget_get_widget (swidget, "details-box");
	g_return_if_fail (widget != NULL);
	gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
    
	seahorse_bind_property_full ("item-attributes", object, transform_item_details, 
	                             "label", widget, NULL);
}

/* -----------------------------------------------------------------------------
 * GENERAL
 */

static void
properties_response (GtkDialog *dialog, int response, SeahorseWidget *swidget)
{
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help(swidget);
        return;
    }
   
    seahorse_widget_destroy (swidget);
}

void
seahorse_gkr_item_properties_show (SeahorseGkrItem *git, GtkWindow *parent)
{
    SeahorseObject *object = SEAHORSE_OBJECT (git);
    SeahorseWidget *swidget = NULL;
    GtkWidget *widget;

    swidget = seahorse_object_widget_new ("gkr-item-properties", parent, object);
    
    /* This happens if the window is already open */
    if (swidget == NULL)
        return;

    seahorse_object_refresh (object);

    widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, swidget->name));
    g_signal_connect (widget, "response", G_CALLBACK (properties_response), swidget);

    /* 
     * The signals don't need to keep getting connected. Everytime a key changes the
     * do_* functions get called. Therefore, seperate functions connect the signals
     * have been created
     */

    setup_main (swidget);
    setup_details (swidget);
}
