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
    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), GTK_WIDGET (box));

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
    gchar **recipients, **k;
    gchar *signer;
    
    recipients = cryptui_prompt_recipients (keyset, "Choose Recipients", &signer);
    
    if (recipients) {
        for (k = recipients; *k; k++)
            g_print ("RECIPIENT: %s\n", *k);
        g_strfreev (recipients);
        g_print ("SIGNER: %s\n", signer);
        g_free (signer);
    }
}

static void
print_keyset (CryptUIKeyset *keyset)
{
    GList *keys, *l;
    gboolean cache = FALSE;
    gchar *name;
    guint flags;
    
    keys = cryptui_keyset_get_keys (keyset);
    
    for(l = keys; l; l = g_list_next (l)) {
        g_print ("key: %s\n", (gchar*)l->data);
        
        /* Test half of them cached, half not */
        if (cache)
            cryptui_keyset_cache_key (keyset, (gchar*)l->data);
        cache = !cache;
        
        name = cryptui_keyset_key_display_name (keyset, (gchar*)l->data);
        g_print ("     name: %s\n", name);
        g_free (name);
        
        name = cryptui_keyset_key_display_id (keyset, (gchar*)l->data);
        g_print ("     id: %s\n", name);
        g_free (name);
        
        flags = cryptui_keyset_key_flags (keyset, (gchar*)l->data);
        g_print ("     flags: %d\n", flags);
    }
}

int
main (int argc, char **argv)
{
    CryptUIKeyset *keyset;
    const gchar *arg = "normal";
    
    gtk_init(&argc, &argv);
    
    keyset = cryptui_keyset_new ("openpgp", TRUE);
    if (argc > 1)
	    arg = argv[1];
	    
    if (g_ascii_strcasecmp (arg, "plain") == 0) 
	    show_ui_dialog (keyset);
    else if (g_ascii_strcasecmp (arg, "normal") == 0)
	    show_chooser_dialog (keyset);
    else if (g_ascii_strcasecmp (arg, "keyset") == 0)
	    print_keyset (keyset);
    else
	    g_warning ("must specify something valid to display");
	    
    return 0;
}
