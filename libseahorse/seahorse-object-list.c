/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
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

#include "seahorse-object-list.h"

GList*
seahorse_object_list_append (GList *original, gpointer object)
{
	g_return_val_if_fail (G_IS_OBJECT (object), original);
	return g_list_append (original, g_object_ref (object));
}

GList*
seahorse_object_list_prepend (GList *original, gpointer object)
{
	g_return_val_if_fail (G_IS_OBJECT (object), original);
	return g_list_prepend (original, g_object_ref (object));	
}

GList*
seahorse_object_list_remove (GList *original, gpointer object)
{
	GList *at;
	
	g_return_val_if_fail (G_IS_OBJECT (object), original);
	
	at = g_list_find (original, object);
	if (at != NULL) {
		g_object_unref (object);
		return g_list_delete_link (original, at); 
	}
	
	return original;
}

GList*
seahorse_object_list_copy (GList *original)
{
	GList *l, *copy;
	
	copy = g_list_copy (original);
	for (l = copy; l; l = g_list_next (l))
		g_object_ref (l->data);
	
	return copy;
}

void
seahorse_object_list_free (gpointer list)
{
	GList *l;

	for (l = list; l; l = g_list_next (l))
		g_object_unref (l->data);
	g_list_free (list);
}

GType
seahorse_object_list_type (void)
{
	static GType type = 0;
	if (!type)
		type = g_boxed_type_register_static ("GList_GObject", 
		                                     (GBoxedCopyFunc)seahorse_object_list_copy,
		                                     (GBoxedFreeFunc)seahorse_object_list_free);
	return type;
}
