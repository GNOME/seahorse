/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#include "seahorse-delete.h"
#include "seahorse-ops-key.h"

static gboolean
warning (GtkWindow *parent, SeahorseKey *skey, const guint index)
{
	GtkWidget *warning;
	gint response;
	const gchar *message, *keyid;
	
	g_return_val_if_fail (skey != NULL && SEAHORSE_IS_KEY (skey), FALSE);
	
	if (index == 0)
		message = _("Are you sure you want to permanently delete key %s, %s?");
	else
		message = _("Are you sure you want to permanently delete subkey %s for %s?");
	
	warning = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
		GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, g_strdup_printf (
		message, seahorse_key_get_keyid (skey, index),
		seahorse_key_get_userid (skey, 0)));
	
	response = gtk_dialog_run (GTK_DIALOG (warning));
	gtk_widget_destroy (warning);
	
	return (response == GTK_RESPONSE_YES);
}

void
seahorse_delete_key_new (GtkWindow *parent, SeahorseContext *sctx, SeahorseKey *skey)
{
	if (warning (parent, skey, 0))
		seahorse_ops_key_delete (sctx, skey);
}

void
seahorse_delete_subkey_new (GtkWindow *parent, SeahorseContext *sctx,
			    SeahorseKey *skey, const guint index)
{
	if (warning (parent, skey, index))
		seahorse_ops_key_del_subkey (sctx, skey, index);
}
