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

static void
show_ui_dialog (CryptUIKeyset *keyset)
{
    GtkWidget *dialog;
    CryptUIKeyCombo *combo;
    CryptUIKeyStore *ckstore;
    
    dialog = gtk_dialog_new_with_buttons ("CryptUI Test", NULL, GTK_DIALOG_MODAL, 
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
    
    ckstore = cryptui_key_store_new (keyset, FALSE);
    combo = cryptui_key_combo_new (ckstore);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), GTK_WIDGET (combo));
    gtk_widget_show_all (dialog);
    
    gtk_dialog_run (GTK_DIALOG (dialog));
}

int
main (int argc, char **argv)
{
    CryptUIKeyset *keyset;
    GList *keys, *l;
    
    gtk_init(&argc, &argv);
    
    keyset = cryptui_keyset_new ("openpgp", CRYPTUI_ENCTYPE_NONE);
    keys = cryptui_keyset_get_keys (keyset);
    
    for(l = keys; l; l = g_list_next (l)) 
        g_print ("key: %s\n", (gchar*)l->data);
    
    show_ui_dialog (keyset);
    /* gtk_main(); */
    
    return 0;
}


