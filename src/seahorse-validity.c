/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include <gnome.h>

#include "seahorse-validity.h"

const gchar*
seahorse_validity_get_string (SeahorseValidity validity)
{
	gchar *string;
	
	switch (validity) {
		case SEAHORSE_VALIDITY_UNKNOWN:
			string = _("Unknown");
			break;
		case SEAHORSE_VALIDITY_NEVER:
			string = _("Never");
			break;
		case SEAHORSE_VALIDITY_MARGINAL:
			string = _("Marginal");
			break;
		case SEAHORSE_VALIDITY_FULL:
			string = _("Full");
			break;
		case SEAHORSE_VALIDITY_ULTIMATE:
			string = _("Ultimate");
			break;
		default:
			string = NULL;
			break;
	}
	
	return string;
}

static guint
get_from_string (const gchar *validity)
{
	if (g_str_equal (validity, "Unknown"))
		return SEAHORSE_VALIDITY_UNKNOWN;
	if (g_str_equal (validity, "Never"))
		return SEAHORSE_VALIDITY_NEVER;
	if (g_str_equal (validity, "Marginal"))
		return SEAHORSE_VALIDITY_MARGINAL;
	if (g_str_equal (validity, "Full"))
		return SEAHORSE_VALIDITY_FULL;
	if (g_str_equal (validity, "Ultimate"))
		return SEAHORSE_VALIDITY_ULTIMATE;
	return 0;
}

gint
seahorse_validity_compare (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, guint index)
{	
	const gchar *a_string, *b_string;
	SeahorseValidity a_valid, b_valid;
	
	gtk_tree_model_get (model, a, index, &a_string, -1);
	gtk_tree_model_get (model, b, index, &b_string, -1);
	
	if (a_string == NULL || b_string == NULL || g_str_equal (a_string, b_string))
		return 0;
	
	a_valid = get_from_string (a_string);
	b_valid = get_from_string (b_string);
	
	if (a_valid > b_valid)
		return -1;
	else if (a_valid < b_valid)
		return 1;
	else
		return 0;
}
