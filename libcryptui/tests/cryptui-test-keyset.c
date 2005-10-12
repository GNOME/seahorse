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
    
    gtk_init(&argc, &argv);
	
    keyset = cryptui_keyset_new ("openpgp", CRYPTUI_ENCTYPE_NONE);
    keys = cryptui_keyset_get_keys (keyset);
    
    for(l = keys; l; l = g_list_next (l)) 
        g_print ("key: %s\n", (gchar*)l->data);

    /* gtk_main(); */
    
	return 0;
}
