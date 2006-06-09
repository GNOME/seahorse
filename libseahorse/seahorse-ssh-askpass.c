/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

#include "config.h"
#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>

#include <libgnomeui/gnome-password-dialog.h>

#ifdef WITH_GNOME_KEYRING
#include <gnome-keyring.h>
#endif

#include "seahorse-ssh-operation.h"

#ifdef WITH_GNOME_KEYRING

#define KEYRING_ATTR_TYPE "seahorse-key-type"
#define KEYRING_ATTR_KEYID "openssh-keyid"
#define KEYRING_VAL_SSH "openssh"

static gchar*
get_keyring_passphrase (const gchar *id)
{
    GnomeKeyringAttributeList *attributes = NULL;
    GnomeKeyringResult res;
    GList *found_items;
    GnomeKeyringFound *found;
    gchar *ret = NULL;
    
    g_assert (id != NULL);
    
    attributes = gnome_keyring_attribute_list_new ();
    gnome_keyring_attribute_list_append_string (attributes, KEYRING_ATTR_KEYID, id);
    res = gnome_keyring_find_items_sync (GNOME_KEYRING_ITEM_GENERIC_SECRET, attributes, 
                                         &found_items);
    gnome_keyring_attribute_list_free (attributes);
        
    if (res != GNOME_KEYRING_RESULT_OK) {
        if (res != GNOME_KEYRING_RESULT_DENIED)
            g_warning ("couldn't search keyring: (code %d)", res);
            
    } else {
        
        if (found_items && found_items->data) {
            found = (GnomeKeyringFound*)found_items->data;
            if (found->secret)
                ret = g_strdup (found->secret);
        }
            
        gnome_keyring_found_list_free (found_items);
    }
    
    return ret;
}

static void 
set_keyring_passphrase (const gchar *id, const gchar *pass, gboolean forever)
{
    const gchar *desc;
    const gchar *keyring = forever ? NULL : "session";
    GnomeKeyringResult res;
    GnomeKeyringAttributeList *attributes = NULL;
    guint item_id;
    
    g_assert (id != NULL);
    desc = g_getenv (SEAHORSE_SSH_ENV_DESC);
        
    attributes = gnome_keyring_attribute_list_new ();
    gnome_keyring_attribute_list_append_string (attributes, KEYRING_ATTR_TYPE, 
                                                KEYRING_VAL_SSH);
    gnome_keyring_attribute_list_append_string (attributes, KEYRING_ATTR_KEYID, id);
    res = gnome_keyring_item_create_sync (keyring, GNOME_KEYRING_ITEM_GENERIC_SECRET, 
                                          desc, attributes, pass, TRUE, &item_id);
    gnome_keyring_attribute_list_free (attributes);
        
    if (res != GNOME_KEYRING_RESULT_OK)
        g_warning ("Couldn't store password in keyring: (code %d)", res);
}

#endif /* WITH_GNOME_KEYRING */

static gchar*
get_passphrase(const gchar *message)
{
    GnomePasswordDialogRemember remember;
    const gchar *title, *id, *desc;
    gchar *ret = NULL;
    gchar *prompt = NULL;
    GtkWidget *dlg;
    const gchar *locale;
    
    locale = g_getenv ("LC_ALL");
    if (locale == NULL)
        locale = g_getenv ("LANG");
    if (locale == NULL)
        locale = "C";
    
    id = g_getenv (SEAHORSE_SSH_ENV_ID);
    
    /* 
     * We can only use custom messages and keyring support if when 
     * in the C locale. This is because we use screen scraping.
     */
    
    if(strcmp (locale, "C") == 0) {

        /* See if we're being reprompted */
        gboolean bad = (g_strncasecmp (message, "bad", 3) == 0);

#ifdef WITH_GNOME_KEYRING
        if (!bad && id) {
            ret = get_keyring_passphrase (id);
            if (ret != NULL)
                return ret;
        }
#endif
        
        desc = g_getenv (SEAHORSE_SSH_ENV_DESC);
        if (desc != NULL) {
            prompt = g_strdup_printf (_("%sEnter passphrase for: %s"), 
                                      bad ? _("The passphrase was incorrect.\n") : "", 
                                      desc);
        }
    }
    
    title = g_getenv (SEAHORSE_SSH_ENV_PROMPT_TITLE);
    if (title == NULL)
        title = _("SSH Passphrase");
    
    dlg = gnome_password_dialog_new (title, prompt ? prompt : message, 
                                     NULL, NULL, TRUE);
    gnome_password_dialog_set_show_username (GNOME_PASSWORD_DIALOG (dlg), FALSE);
    gnome_password_dialog_set_show_domain (GNOME_PASSWORD_DIALOG (dlg), FALSE);
    gnome_password_dialog_set_show_remember (GNOME_PASSWORD_DIALOG (dlg), FALSE);
    
#ifdef WITH_GNOME_KEYRING
    if (id) { 
        gnome_password_dialog_set_show_remember (GNOME_PASSWORD_DIALOG (dlg), TRUE);
        gnome_password_dialog_set_remember (GNOME_PASSWORD_DIALOG (dlg), GNOME_PASSWORD_DIALOG_REMEMBER_FOREVER);
        gnome_password_dialog_set_remember (GNOME_PASSWORD_DIALOG (dlg), GNOME_PASSWORD_DIALOG_REMEMBER_SESSION);
    }
#endif
    
    gnome_password_dialog_set_show_userpass_buttons (GNOME_PASSWORD_DIALOG (dlg), FALSE);
    
    if (gnome_password_dialog_run_and_block (GNOME_PASSWORD_DIALOG (dlg))) {
        ret = gnome_password_dialog_get_password (GNOME_PASSWORD_DIALOG (dlg));
        
#ifdef WITH_GNOME_KEYRING
        remember = gnome_password_dialog_get_remember (GNOME_PASSWORD_DIALOG (dlg));
        if (id && remember != GNOME_PASSWORD_DIALOG_REMEMBER_NOTHING)
            set_keyring_passphrase (id, ret, remember & GNOME_PASSWORD_DIALOG_REMEMBER_FOREVER);
#endif
    }
    
    g_free (prompt);
    gtk_widget_destroy (dlg);
    return ret;
}

int main (int argc, char* argv[])
{
    gchar *pass, *p;
    gchar *message;
    int ret = 0;
    
    gtk_init (&argc, &argv);

    /* Non buffered stdout */
    setvbuf(stdout, 0, _IONBF, 0);
    
    if (argc > 1)
        message = g_strjoinv(" ", argv + 1);
    else 
        message = g_strdup("Enter your SSH passphrase:");

    pass = get_passphrase (message);
    if (pass != NULL) {
        write (1, pass, strlen (pass));
        for (p = pass; *p; p++) 
            *p = 0;
        g_free (pass);
    }
    
    g_free (message);
    
    return ret;
}
