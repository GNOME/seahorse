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

#include <libintl.h>
#include <gnome.h>

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-operation.h"
#include "seahorse-windows.h"
#include "seahorse-progress.h"
#include "seahorse-gtkstock.h"
#include "seahorse-ssh-source.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"

static void 
upload_complete (SeahorseOperation *op, gpointer dummy)
{
    GError *err = NULL;
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't configure Secure Shell keys on remote computer."));
    }    
}

static void
input_changed (GtkWidget *dummy, SeahorseWidget *swidget)
{
    GtkWidget *widget;
    const gchar *user, *host;

    widget = glade_xml_get_widget (swidget->xml, "user-label");
    user = gtk_entry_get_text (GTK_ENTRY (widget));

    widget = glade_xml_get_widget (swidget->xml, "host-label");
    host = gtk_entry_get_text (GTK_ENTRY (widget));

    widget = glade_xml_get_widget (swidget->xml, "ok");
    gtk_widget_set_sensitive (widget, user && user[0] && host && host[0]);
}


static SeahorseOperation*
upload_via_source (const gchar *user, const gchar *host, GList *keys)
{
    SeahorseMultiOperation *mop = NULL;
    SeahorseOperation *op = NULL;
    SeahorseKeySource *sksrc;
    SeahorseKey *skey;
    GList *next;
    
    seahorse_util_keylist_sort (keys);
    g_assert (keys);
    
    while (keys) {
     
        /* Break off one set (same keysource) */
        next = seahorse_util_keylist_splice (keys);
        
        g_assert (SEAHORSE_IS_KEY (keys->data));
        skey = SEAHORSE_KEY (keys->data);

        /* Upload via this key source */        
        sksrc = seahorse_key_get_source (skey);
        g_return_val_if_fail (sksrc != NULL, NULL);
        
        g_return_val_if_fail (SEAHORSE_IS_SSH_SOURCE (sksrc), NULL);

        /* If more than one operation start combining */
        if (op != NULL) {
            g_assert (mop == NULL);
            mop = seahorse_multi_operation_new ();
            seahorse_multi_operation_take (mop, op);
        }

        /* Start an upload process */
        op = seahorse_ssh_operation_upload (SEAHORSE_SSH_SOURCE (sksrc), keys, user, host);
        g_return_val_if_fail (op != NULL, NULL);

        /* And combine if necessary */
        if (mop != NULL) {
            seahorse_multi_operation_take (mop, op);
            op = NULL;
        }
        
        g_list_free (keys);
        keys = next;
    }
    
    return mop ? SEAHORSE_OPERATION (mop) : op;
}

static void
upload_keys (SeahorseWidget *swidget)
{
    SeahorseOperation *op;
    GtkWidget *widget;
    const gchar *user, *host;
    GList *keys;
    gchar *t;

    widget = glade_xml_get_widget (swidget->xml, "user-label");
    user = gtk_entry_get_text (GTK_ENTRY (widget));
    g_return_if_fail (user && user[0]);
    
    widget = glade_xml_get_widget (swidget->xml, "host-label");
    host = gtk_entry_get_text (GTK_ENTRY (widget));
    g_return_if_fail (host && host[0]);

    keys = (GList*)g_object_steal_data (G_OBJECT (swidget), "upload-keys");
    g_return_if_fail (keys != NULL);
    
    /* This frees |keys| */
    op = upload_via_source (user, host, keys);
    g_return_if_fail (op != NULL);
    
    seahorse_operation_watch (op, G_CALLBACK (upload_complete), NULL, NULL);
    
    /* Show the progress window if necessary */
    seahorse_progress_show (op, _("Configuring Secure Shell Keys..."), FALSE);
    g_object_unref (op);
}

/**
 * seahorse_upload_show
 * @keys: Upload a certain set of SSH keys
 * 
 * Prompt a dialog to upload keys.
 **/
void
seahorse_ssh_upload_prompt (GList *keys)
{
    SeahorseWidget *swidget;
    GtkWindow *win;
    GtkWidget *w;
    
    g_return_if_fail (keys != NULL);
    
    swidget = seahorse_widget_new_allow_multiple ("ssh-upload");
    g_return_if_fail (swidget != NULL);
    
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));

    /* Set the image properly */
    w = glade_xml_get_widget (swidget->xml, "icon-image");
    gtk_image_set_from_stock (GTK_IMAGE (w), SEAHORSE_STOCK_KEY_SSH, 
                              GTK_ICON_SIZE_DIALOG);

    /* Default to the users current name */
    w = glade_xml_get_widget (swidget->xml, "user-label");
    gtk_entry_set_text (GTK_ENTRY (w), g_get_user_name ());
 
    /* Focus the host */
    w = glade_xml_get_widget (swidget->xml, "host-label");
    gtk_widget_grab_focus (w);    

    keys = g_list_copy (keys);
    g_object_set_data_full (G_OBJECT (swidget), "upload-keys", keys, 
                            (GDestroyNotify)g_list_free);

    glade_xml_signal_connect_data (swidget->xml, "input_changed", 
                                   G_CALLBACK (input_changed), swidget);

    input_changed (NULL, swidget);

    for (;;) {
        switch (gtk_dialog_run (GTK_DIALOG (win))) {
        case GTK_RESPONSE_HELP:
            /* TODO: Help */
            continue;
        case GTK_RESPONSE_ACCEPT:
            upload_keys (swidget);
            break;
        default:
            break;
        };
        
        break;
    }
    
    seahorse_widget_destroy (swidget);
}
