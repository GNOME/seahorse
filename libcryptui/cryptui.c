/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Nate Nielsen 
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
#include <string.h>

#include <dbus/dbus-glib-bindings.h>

#include "cryptui.h"
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
    
    remote_service = dbus_g_proxy_new_for_name (bus, "org.gnome.seahorse.KeyService", 
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

CryptUIEncType
cryptui_key_get_enctype (const gchar *key)
{
    init_remote_service ();
    
    /* TODO: Implement this properly */
    return CRYPTUI_ENCTYPE_PUBLIC;
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
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), GTK_WIDGET (chooser));
    
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
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 5);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), GTK_WIDGET (chooser));
    
    gtk_widget_show_all (dialog);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        t = cryptui_key_chooser_get_signer (chooser);
        signer = t ? g_strdup (t) : NULL;
    }
    
    gtk_widget_destroy (dialog);
    return signer;
}
