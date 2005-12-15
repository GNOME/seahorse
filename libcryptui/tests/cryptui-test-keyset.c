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
 
#include "cryptui.h"
#include "cryptui-keyset.h"

#include <gtk/gtk.h>

int
main (int argc, char **argv)
{
    CryptUIKeyset *keyset;
    GList *keys, *l;
    gboolean cache = FALSE;
    gchar *name;
    
    gtk_init(&argc, &argv);

    keyset = cryptui_keyset_new ("openpgp");
    g_object_set (keyset, "expand-keys", TRUE, NULL);
    
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
    }

    /* gtk_main(); */
    
    return 0;
}
