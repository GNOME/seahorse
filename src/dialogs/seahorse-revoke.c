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
 
#include "seahorse-revoke.h"
#include "seahorse-key-widget.h"
#include "seahorse-ops-key.h"

#define SUBKEY "subkey"
#define REASON "reason"
#define DESC "description"

static void
key_ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	
}

static void
subkey_ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
	guint index;
	SeahorseRevokeReason reason;
	const gchar *description;
	
	index = (guint)g_object_steal_data (G_OBJECT (swidget), SUBKEY);
	reason = gtk_option_menu_get_history (GTK_OPTION_MENU (
		glade_xml_get_widget (swidget->xml, REASON)));
	description = gtk_entry_get_text (GTK_ENTRY (
		glade_xml_get_widget (swidget->xml, DESC)));
	
	seahorse_ops_key_revoke_subkey (swidget->sctx,
		SEAHORSE_KEY_WIDGET (swidget)->skey, index, reason, description);
	seahorse_widget_destroy (swidget);
}

static SeahorseWidget*
revoke_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	SeahorseWidget *swidget;
	
	swidget = seahorse_key_widget_new ("revoke", sctx, skey);
	g_return_if_fail (swidget != NULL);
	
	return swidget;
}

void
seahorse_revoke_key_new (SeahorseContext *sctx, SeahorseKey *skey)
{
	return;
}

void
seahorse_revoke_subkey_new (SeahorseContext *sctx,
			    SeahorseKey *skey, const guint index)
{
	SeahorseWidget *swidget;
	
	g_return_if_fail ((swidget = revoke_new (sctx, skey)) != NULL);
	
	g_object_set_data (G_OBJECT (swidget), SUBKEY, (gpointer)index);
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		G_CALLBACK (subkey_ok_clicked), swidget);
}
