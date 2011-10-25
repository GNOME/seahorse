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
#include "seahorse-actions.h"

#include <gck/gck.h>

static GQuark   seahorse_action_info_quark (void) G_GNUC_CONST;

static GQuark
seahorse_action_info_quark (void)
{
	static GQuark quark = 0;
	if (quark == 0)
		quark = g_quark_from_static_string ("seahorse-action-window");
	return quark;
}

void
seahorse_action_pre_activate_with_window (GtkAction *action,
                                          GtkWindow *window)
{
	g_return_if_fail (GTK_IS_ACTION (action));
	g_return_if_fail (window != NULL || GTK_IS_WINDOW (window));

	g_object_set_qdata_full (G_OBJECT (action), seahorse_action_info_quark (),
	                         window ? g_object_ref (window) : NULL, g_object_unref);
}

void
seahorse_action_activate_with_window (GtkAction *action,
                                      GtkWindow *window)
{
	g_return_if_fail (GTK_IS_ACTION (action));
	g_return_if_fail (window != NULL || GTK_IS_WINDOW (window));

	g_object_ref (action);

	seahorse_action_pre_activate_with_window (action, window);
	gtk_action_activate (action);
	seahorse_action_post_activate (action);

	g_object_unref (action);
}

void
seahorse_action_post_activate (GtkAction *action)
{
	g_return_if_fail (GTK_IS_ACTION (action));

	g_object_set_qdata (G_OBJECT (action), seahorse_action_info_quark (), NULL);
}


GtkWindow *
seahorse_action_get_window (GtkAction *action)
{
	g_return_val_if_fail (GTK_IS_ACTION (action), NULL);
	return g_object_get_qdata (G_OBJECT (action), seahorse_action_info_quark ());
}
