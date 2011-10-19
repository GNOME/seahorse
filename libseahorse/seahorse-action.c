/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "seahorse-action.h"

#include <gck/gck.h>

typedef struct {
	GtkWindow *window;
	GList *objects;
} SeahorseActionInfo;

static void
seahorse_action_info_free (gpointer data)
{
	SeahorseActionInfo *info = data;
	g_clear_object (&info->window);
	gck_list_unref_free (info->objects);
	g_slice_free (SeahorseActionInfo, info);
}

static SeahorseActionInfo *
seahorse_action_info_lookup (GtkAction *action)
{
	SeahorseActionInfo *info;
	static GQuark QUARK_INFO = 0;

	if (QUARK_INFO == 0)
		QUARK_INFO = g_quark_from_static_string ("seahorse-action-info");

	info = g_object_get_qdata (G_OBJECT (action), QUARK_INFO);
	if (info == NULL) {
		info = g_slice_new0 (SeahorseActionInfo);
		g_object_set_qdata_full (G_OBJECT (action), QUARK_INFO, info,
		                         seahorse_action_info_free);
	}

	return info;
}

GtkWindow *
seahorse_action_get_window (GtkAction *action)
{
	SeahorseActionInfo *info;

	g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

	info = seahorse_action_info_lookup (action);
	return info->window;
}

gpointer
seahorse_action_get_object (GtkAction *action)
{
	SeahorseActionInfo *info;

	g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

	info = seahorse_action_info_lookup (action);
	return info->objects ? info->objects->data : NULL;
}

GList *
seahorse_action_get_objects (GtkAction *action)
{
	SeahorseActionInfo *info;

	g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

	info = seahorse_action_info_lookup (action);
	return info->objects;
}

void
seahorse_action_set_window (GtkAction *action,
                            GtkWindow *window)
{
	SeahorseActionInfo *info;

	g_return_if_fail (GTK_IS_ACTION (action));

	info = seahorse_action_info_lookup (action);
	g_clear_object (&info->window);
	info->window = window ? g_object_ref (window) : NULL;
}

void
seahorse_action_set_objects (GtkAction *action,
                             GList *objects)
{
	SeahorseActionInfo *info;

	g_return_if_fail (GTK_IS_ACTION (action));

	info = seahorse_action_info_lookup (action);
	gck_list_unref_free (info->objects);
	info->objects = gck_list_ref_copy (objects);
}

gboolean
seahorse_action_have_objects (GtkAction *action)
{
	SeahorseActionInfo *info;

	g_return_val_if_fail (GTK_IS_ACTION (action), FALSE);

	info = seahorse_action_info_lookup (action);
	return info->objects ? TRUE : FALSE;
}
