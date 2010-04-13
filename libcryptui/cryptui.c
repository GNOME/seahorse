/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Stefan Walter
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
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>

#include <gconf/gconf-client.h>
#include <dbus/dbus-glib-bindings.h>

#include "cryptui.h"
#include "cryptui-priv.h"
#include "cryptui-key-chooser.h"
#include "cryptui-keyset.h"

DBusGProxy *remote_service = NULL;

static gboolean
init_remote_service ()
{
    DBusGConnection *bus;
    GError *error = NULL;
    
    if (remote_service) 
        return TRUE;

    bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
    if (!bus) {
        g_critical ("couldn't get the session bus: %s", error->message);
        g_clear_error (&error);
        return FALSE;
    }
    
    remote_service = dbus_g_proxy_new_for_name (bus, "org.gnome.seahorse", 
                              "/org/gnome/seahorse/keys", "org.gnome.seahorse.KeyService");
            
    if (!remote_service) {
        g_critical ("couldn't connect to the dbus service");
        return FALSE;
    }
    
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

/**
 * cryptui_key_get_base:
 * @key: (in): key for use with libcryptui
 *
 * This function is a utility function to get the part of the key that preceeds
 * the colon.
 *
 * Returns: the key base if one is found in @key
 *          or @key if it is already a base or is invalid
 */
gchar*
cryptui_key_get_base (const gchar *key)
{
    gchar *ch;
    
    /* Anything past the second colon is stripped */
    ch = strchr (key, ':');
    if (ch) {
        ch = strchr (ch + 1, ':');
        if (ch)
            return g_strndup (key, ch - key);
    }
    
    /* Either already base or invalid */
    return g_strdup (key);
}

/**
 * cryptui_key_get_enctype:
 * @key: (in): key for use with libcryptui
 *
 * A utility function to get the type of key passed in. (public, private,
 * symmetric, etc.).
 *
 * Returns: The type of key.
 */
CryptUIEncType
cryptui_key_get_enctype (const gchar *key)
{
    init_remote_service ();
    
    /* TODO: Implement this properly */
    return CRYPTUI_ENCTYPE_PUBLIC;
}

/**
 * cryptui_display_notification:
 * @title: (in) (allow-none): Headline for the notification
 * @body: (in) (allow-none): Text for the body of the notification
 * @icon: (in) (allow-none): Full path to icon to be included
 * @urgent: (in) (allow-none): Whether the notification is urgent or not.
 * 
 * This function creates a notification bubble that can be updated as additional
 * key details are discovered.  See http://live.gnome.org/Seahorse/DBus for a
 * description of the markup syntax.
 */
void
cryptui_display_notification (const gchar *title, const gchar *body, const gchar *icon, 
                              gboolean urgent)
{
    GError *error = NULL;
    
    if (!init_remote_service ())
        g_return_if_reached ();
    
    if (!dbus_g_proxy_call (remote_service, "DisplayNotification", &error,
                            G_TYPE_STRING, title, 
                            G_TYPE_STRING, body, 
                            G_TYPE_STRING, icon,
                            G_TYPE_BOOLEAN, urgent,
                            G_TYPE_INVALID,
                            G_TYPE_INVALID)) {
        g_warning ("dbus call DisplayNotification failed: %s", error ? error->message : "");
        g_clear_error (&error);
    }
}

/* -----------------------------------------------------------------------------
 * QUICK PROMPTS
 */

static void 
selection_changed (CryptUIKeyChooser *chooser, GtkWidget *dialog)
{
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, 
                                       cryptui_key_chooser_have_recipients (chooser));
}

/**
 * cryptui_prompt_recipients:
 * @keyset: CryptUIKeyset to select keys to present to the user from
 * @title: Window title for presented GtkWindow
 * @signer: Variable in which to store the key of the signer if one is selected
 *
 * This function prompts the user to select one or more keys from the keyset to
 * use to encrypt to.  It also allows the user to select a private key from the
 * keyset to sign with. 
 *
 * Returns: the selected key
 */
 
gchar**
cryptui_prompt_recipients (CryptUIKeyset *keyset, const gchar *title, 
                           gchar **signer)
{
    CryptUIKeyChooser *chooser;
    GtkWidget *dialog;
    gchar **keys = NULL;
    GList *recipients, *l;
    guint mode = CRYPTUI_KEY_CHOOSER_RECIPIENTS;
    const gchar *t;
    int i;
    
    if (signer) {
        *signer = NULL;
        mode |= CRYPTUI_KEY_CHOOSER_SIGNER;
    }
    
    dialog = gtk_dialog_new_with_buttons (title, NULL, GTK_DIALOG_MODAL, 
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, 
                                          NULL);
    
    chooser = cryptui_key_chooser_new (keyset, mode);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), GTK_WIDGET (chooser));
    gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
    
    g_signal_connect (chooser, "changed", G_CALLBACK (selection_changed), dialog);
    selection_changed (chooser, dialog);
    
    gtk_widget_show_all (dialog);
    
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        
        recipients = cryptui_key_chooser_get_recipients (chooser);
        keys = g_new0(gchar*, g_list_length (recipients) + 1);
        for (l = recipients, i = 0; l; l = g_list_next (l), i++)
            keys[i] = g_strdup (l->data);
        g_list_free (recipients);
        
        if (signer) {
            t = cryptui_key_chooser_get_signer (chooser);
            *signer = t ? g_strdup (t) : NULL;
        }
    }
    
    gtk_widget_destroy (dialog);
    return keys;
}

/**
 * cryptui_prompt_signer:
 * @keyset: CryptUIKeyset to select keys to present to the user from
 * @title: Window title for presented GtkWindow
 *
 * This function prompts the user to select a private key from the keyset to
 * use to sign something.
 *
 * Returns: the selected key
 */
gchar*
cryptui_prompt_signer (CryptUIKeyset *keyset, const gchar *title)
{
    CryptUIKeyChooser *chooser;
    GtkWidget *dialog;
    gchar *signer = NULL;
    const gchar *t;
    
    dialog = gtk_dialog_new_with_buttons (title, NULL, GTK_DIALOG_MODAL, 
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, 
                                          NULL);
    
    chooser = cryptui_key_chooser_new (keyset, CRYPTUI_KEY_CHOOSER_SIGNER |
                                               CRYPTUI_KEY_CHOOSER_MUSTSIGN);
    gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 5);
    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), GTK_WIDGET (chooser));
    gtk_window_set_default_size (GTK_WINDOW (dialog), 400, -1);
    
    gtk_widget_show_all (dialog);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        t = cryptui_key_chooser_get_signer (chooser);
        signer = t ? g_strdup (t) : NULL;
    }
    
    gtk_widget_destroy (dialog);
    return signer;
}

/**
 * cryptui_need_to_get_keys:
 *
 * This function is called when seahorse needs to be launched to generate a
 * key or keys or import a key or keys to perform the requested operation.
 */
void                
cryptui_need_to_get_keys ()
{
    GtkWidget *dialog;
    gchar *argv[2] = {"seahorse", NULL};

    dialog = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                          _("No encryption keys were found with which to perform the operation you requested.  The program <b>Passwords and Encryption Keys</b> will now be started so that you may either create a key or import one."));
               
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
        gtk_widget_destroy (dialog);    
        
        g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
    }
}

/* -----------------------------------------------------------------------------
 * GCONF HELPERS 
 */

static GConfClient *global_gconf_client = NULL;

static void
global_client_free (void)
{
    if (global_gconf_client == NULL)
        return;
    
    gconf_client_remove_dir (global_gconf_client, SEAHORSE_DESKTOP_KEYS, NULL);
    g_object_unref (global_gconf_client);
    global_gconf_client = NULL;
}

static gboolean
handle_error (GError **error)
{
    g_return_val_if_fail (error != NULL, FALSE);

    if (*error != NULL) {
        g_warning ("GConf error:\n  %s", (*error)->message);
        g_error_free (*error);
        *error = NULL;
        return TRUE;
    }

    return FALSE;
}

static GConfClient*
get_global_client (void)
{
    GError *error = NULL;
    
    if (global_gconf_client != NULL)
        return global_gconf_client;
    
    global_gconf_client = gconf_client_get_default ();
    if (global_gconf_client) {
        gconf_client_add_dir (global_gconf_client, SEAHORSE_DESKTOP_KEYS, 
                                  GCONF_CLIENT_PRELOAD_NONE, &error);
        handle_error (&error);
    }

    atexit (global_client_free);
    return global_gconf_client;
}

/**
 * _cryptui_gconf_get_boolean:
 * @key: a gconf key/path
 *
 * A private library function that gets a boolean from a gconf key.
 *
 * Returns: The boolean stored at the key location or FALSE if an error
 *          occured.
 */
gboolean
_cryptui_gconf_get_boolean (const char *key)
{
    GConfClient *client = get_global_client ();
    gboolean result;
    GError *error = NULL;
    
    g_return_val_if_fail (key != NULL, FALSE);
    g_return_val_if_fail (client != NULL, FALSE);
    
    result = gconf_client_get_bool (client, key, &error);
    return handle_error (&error) ? FALSE : result;
}

/**
 * _cryptui_gconf_get_string:
 * @key: a gconf key/path
 *
 * A private library function that gets a string from a gconf key.
 *
 * Returns: The string stored at the key location or the null string if an error
 *          occured.
 */
char *
_cryptui_gconf_get_string (const char *key)
{
    char *result;
    GConfClient *client;
    GError *error = NULL;
    
    g_return_val_if_fail (key != NULL, NULL);
    
    client = get_global_client ();
    g_return_val_if_fail (client != NULL, NULL);
    
    result = gconf_client_get_string (client, key, &error);
    return handle_error (&error) ? g_strdup ("") : result;
}

/**
 * _cryptui_gconf_set_string:
 * @key: a gconf key/path
 * @string_value: a text string
 *
 * A private library function that sets a gconf key to the string given.
 */
void
_cryptui_gconf_set_string (const char *key, const char *string_value)
{
    GConfClient *client = get_global_client ();
    GError *error = NULL;

    g_return_if_fail (key != NULL);
    g_return_if_fail (client != NULL);
    
    gconf_client_set_string (client, key, string_value, &error);
    handle_error (&error);
}

/**
 * _cryptui_gconf_notify:
 * @key: a gconf key/path
 * @notification_callback: function to be called by the notification
 * @callback_data: data to be passed to the callback function
 *
 * A private library convenience function that creates a gconf notification
 * on a specified gconf key.
 *
 * Returns: the gconf notification id
 */
guint
_cryptui_gconf_notify (const char *key, GConfClientNotifyFunc notification_callback,
                       gpointer callback_data)
{
    GConfClient *client = get_global_client ();
    guint notification_id;
    GError *error = NULL;
    
    g_return_val_if_fail (key != NULL, 0);
    g_return_val_if_fail (notification_callback != NULL, 0);
    g_return_val_if_fail (client != NULL, 0);
    
    notification_id = gconf_client_notify_add (client, key, notification_callback,
                                               callback_data, NULL, &error);
    return handle_error (&error) ? notification_id : 0;
}

static void
internal_gconf_unnotify (gpointer data)
{
    guint notify_id = GPOINTER_TO_INT (data);
    _cryptui_gconf_unnotify (notify_id);
}

/**
 * _cryptui_gconf_notify_lazy:
 * @key: a gconf key/path
 * @notification_callback: function to be called by the notification
 * @callback_data: data to be passed to the callback function
 * @lifetime: the object whose destruction will end the notification
 *
 * A private library convenience function that creates a gconf notification
 * on a specified gconf key and automatically removes the notification when an
 * object is destroyed.
 */
void
_cryptui_gconf_notify_lazy (const char *key, GConfClientNotifyFunc notification_callback,
                            gpointer callback_data, gpointer lifetime)
{
    guint notification_id;
    gchar *t;
    
    notification_id = _cryptui_gconf_notify (key, notification_callback, callback_data);
    if (notification_id != 0) {
        t = g_strdup_printf ("_cryutui-gconf-notify-lazy-%d", notification_id);
        g_object_set_data_full (G_OBJECT (lifetime), t, 
                GINT_TO_POINTER (notification_id), internal_gconf_unnotify);
    }
}

/**
 * _crytpui_gconf_unnotify:
 * @notification_id: a current gconf notification id
 *
 * A private library function to remove a gconf notification.
 */
void
_cryptui_gconf_unnotify (guint notification_id)
{
    GConfClient *client = get_global_client ();
    g_return_if_fail (client != NULL);

    if (notification_id == 0)
        return;

    gconf_client_notify_remove (client, notification_id);
}
