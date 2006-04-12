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
#include <gtk/gtk.h>

#include "cryptui.h"
#include "cryptui-keyset.h"
#include "cryptui-key-store.h"
#include "cryptui-key-combo.h"
#include "cryptui-key-list.h"
#include "cryptui-key-chooser.h"

static void
show_ui_dialog (CryptUIKeyset *keyset)
{
    GtkWidget *dialog;
    GtkComboBox *combo;
    GtkTreeView *list;
    GtkContainer *box;
    CryptUIKeyStore *combo_store;
    CryptUIKeyStore *list_store;
    
    dialog = gtk_dialog_new_with_buttons ("CryptUI Test", NULL, GTK_DIALOG_MODAL, 
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
    
    box = GTK_CONTAINER (gtk_vbox_new (FALSE, 6));
    gtk_container_set_border_width (box, 6);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), GTK_WIDGET (box));

    list_store = cryptui_key_store_new (keyset, TRUE, NULL);
    list = cryptui_key_list_new (list_store, CRYPTUI_KEY_LIST_CHECKS);
    gtk_container_add (box, GTK_WIDGET (list));
    
    combo_store = cryptui_key_store_new (keyset, FALSE, "No Key Selected");
    combo = cryptui_key_combo_new (combo_store);
    gtk_container_add (box, GTK_WIDGET (combo));
    
    gtk_widget_show_all (dialog);    
    gtk_dialog_run (GTK_DIALOG (dialog));
    
    g_object_unref (combo_store);
    g_object_unref (list_store);
}

static void
show_chooser_dialog (CryptUIKeyset *keyset)
{
    CryptUIKeyChooser *chooser;
    GtkWidget *dialog;
    GList *recipients, *l;
    const gchar *signer;
    
    dialog = gtk_dialog_new_with_buttons ("CryptUI Test", NULL, GTK_DIALOG_MODAL, 
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
    
    chooser = cryptui_key_chooser_new (keyset);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), GTK_WIDGET (chooser));
    
    gtk_widget_show_all (dialog);
    gtk_dialog_run (GTK_DIALOG (dialog));
    
    recipients = cryptui_key_chooser_get_recipients (chooser);
    for (l = recipients; l; l = g_list_next (l))
        g_print ("RECIPIENT: %s\n", (char*)(l->data));
    g_list_free (recipients);
    
    signer = cryptui_key_chooser_get_signer (chooser);
    g_print ("SIGNER: %s\n", signer);
    
    gtk_widget_destroy (dialog);
}

int
main (int argc, char **argv)
{
    CryptUIKeyset *keyset;
    
    gtk_init(&argc, &argv);
    
    keyset = cryptui_keyset_new ("openpgp");
    
    if (argc > 1) {
        if (g_ascii_strcasecmp (argv[1], "plain")) {
            show_ui_dialog (keyset);
            return 0;
        } 
    }

    /* The default */
    show_chooser_dialog (keyset);
    return 0;
}
