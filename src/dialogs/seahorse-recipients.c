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

#include "seahorse-recipients.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-ops-key.h"
#include "seahorse-validity.h"
#include "seahorse-recipients-store.h"

#define ALL "all_keys"
#define RECIPIENTS "recipient_keys"

enum {
	PROP_0,
	PROP_VALIDITY
};

static void	seahorse_recipients_class_init		(SeahorseRecipientsClass	*klass);
static void	seahorse_recipients_init		(SeahorseRecipients		*srecips);
static void	seahorse_recipients_finalize		(GObject			*gobject);
static void	seahorse_recipients_set_property	(GObject			*object,
							 guint				prop_id,
							 const GValue			*value,
							 GParamSpec			*pspec);
static void	seahorse_recipients_get_property	(GObject			*object,
							 guint				prop_id,
							 GValue				*value,
							 GParamSpec			*pspec);

static SeahorseWidgetClass	*parent_class		= NULL;

GType
seahorse_recipients_get_type (void)
{
	static GType recipients_type = 0;
	
	if (!recipients_type) {
		static const GTypeInfo recipients_info =
		{
			sizeof (SeahorseRecipientsClass),
			NULL, NULL,
			(GClassInitFunc) seahorse_recipients_class_init,
			NULL, NULL,
			sizeof (SeahorseRecipients),
			0,
			(GInstanceInitFunc) seahorse_recipients_init
		};
		
		recipients_type = g_type_register_static (SEAHORSE_TYPE_WIDGET,
			"SeahorseRecipients", &recipients_info, 0);
	}
	
	return recipients_type;
}

static void
seahorse_recipients_class_init (SeahorseRecipientsClass *klass)
{
	GObjectClass *gobject_class;
	
	parent_class = g_type_class_peek_parent (klass);
	gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->set_property = seahorse_recipients_set_property;
	gobject_class->get_property = seahorse_recipients_get_property;
	gobject_class->finalize = seahorse_recipients_finalize;
	
	g_object_class_install_property (gobject_class, PROP_VALIDITY,
		g_param_spec_boolean ("validity", _("If need validity"),
				      _("TRUE if recipients need to have full validity"),
				      FALSE, G_PARAM_READWRITE));
}

/* Initialize recipient list */
static void
seahorse_recipients_init (SeahorseRecipients *srecips)
{
	if (gpgme_recipients_new (&(srecips->recips)) != GPGME_No_Error)
		srecips->recips = NULL;
}

static void
seahorse_recipients_finalize (GObject *gobject)
{
	SeahorseRecipients *srecips;
	
	srecips = SEAHORSE_RECIPIENTS (gobject);
	seahorse_key_store_destroy (srecips->all_keys);
	seahorse_key_store_destroy (srecips->recipient_keys);
	
	G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
seahorse_recipients_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	SeahorseRecipients *srecips;
	
	srecips = SEAHORSE_RECIPIENTS (object);
	
	switch (prop_id) {
		case PROP_VALIDITY:
			srecips->need_validity = g_value_get_boolean (value);
			break;
		default:
			break;
	}
}

static void
seahorse_recipients_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	SeahorseRecipients *srecips;
	
	srecips = SEAHORSE_RECIPIENTS (object);
	
	switch (prop_id) {
		case PROP_VALIDITY:
			g_value_set_boolean (value, srecips->need_validity);
			break;
		default:
			break;
	}
}

/* Moves key iter from right (non-recips) to left (recips) */
static void
add_recip (SeahorseWidget *swidget, SeahorseKeyRow *skrow)
{
	SeahorseRecipients *srecips;
	GpgmeValidity validity;
	GtkWidget *question;
	gint response;
	
	srecips = SEAHORSE_RECIPIENTS (swidget);
	
	/* Check if need key that can encrypt */
	if (srecips->need_validity && !gpgme_key_get_ulong_attr (skrow->skey->key, GPGME_ATTR_CAN_ENCRYPT, NULL, 0)) {
		seahorse_util_show_error (GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
			_("This key does not have encryption capability.\nIt cannot be added to the list."));
		return;
	}
	
	validity = gpgme_key_get_ulong_attr (skrow->skey->key, GPGME_ATTR_VALIDITY, NULL, 0);
	/* Check if need key to be fully valid */
	if (srecips->need_validity && validity < GPGME_VALIDITY_FULL) {
		question = gtk_message_dialog_new (
			GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name)),
			GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
			"%s\nis not a fully valid key.\n"
			"Would you like to temporarily give it full validity for this operation?",
			seahorse_key_get_userid (skrow->skey, 0));
		
		response = gtk_dialog_run (GTK_DIALOG (question));
		gtk_widget_destroy (question);
		
		/* If don't want key, return */
		if (response != GTK_RESPONSE_YES)
			return;
	}
	
	skrow = seahorse_key_row_transfer (skrow, srecips->recipient_keys);
	
	seahorse_ops_key_recips_add (srecips->recips, skrow->skey);
}

/* Moves key from left (recips) to right (non-recips) */
static void
remove_recip (SeahorseWidget *swidget, SeahorseKeyRow *skrow)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	SeahorseRecipients *srecips;
	
	srecips = SEAHORSE_RECIPIENTS (swidget);
	skrow = seahorse_key_row_transfer (skrow, srecips->all_keys);
	
	model = GTK_TREE_MODEL (srecips->recipient_keys);
	
	/* Need to rebuild recipients */
	gpgme_recipients_release (srecips->recips);
	g_return_if_fail (gpgme_recipients_new (&(srecips->recips)) == GPGME_No_Error);
	
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gtk_tree_model_get (model, &iter, 0, skrow, -1);
			seahorse_ops_key_recips_add (srecips->recips, skrow->skey);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

/* Add recipient button clicked */
static void
add_clicked (GtkButton *button, SeahorseWidget *swidget){
	add_recip (swidget, seahorse_key_store_get_selected_row (
		GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, ALL))));
}

/* Remove recipient button clicked */
static void
remove_clicked (GtkButton *button, SeahorseWidget *swidget){
	remove_recip (swidget, seahorse_key_store_get_selected_row (
		GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, RECIPIENTS))));
}

/* Right (non-recip) key activated */
static void
add_row_activated (GtkTreeView *view, GtkTreePath *path,
		   GtkTreeViewColumn *arg2, SeahorseWidget *swidget)
{
	add_recip (swidget, seahorse_key_store_get_row_from_path (view, path));
}

/* Left (recip) key activated */
static void
remove_row_activated (GtkTreeView *view, GtkTreePath *path,
		      GtkTreeViewColumn *arg2, SeahorseWidget *swidget)
{
	remove_recip (swidget, seahorse_key_store_get_row_from_path (view, path));
}

SeahorseWidget*
seahorse_recipients_new (SeahorseContext *sctx, gboolean need_validity)
{
	SeahorseRecipients *srecips;
	SeahorseWidget *swidget;
	GtkTreeView *all_keys;
	GtkTreeView *recips_keys;
	GtkWidget *recipients;
	gboolean done = FALSE;
	
	srecips = g_object_new (SEAHORSE_TYPE_RECIPIENTS, "name", "recipients", "ctx", sctx,
		"validity", need_validity, NULL);
	swidget = SEAHORSE_WIDGET (srecips);
	
	glade_xml_signal_connect_data (swidget->xml, "add_clicked",
		G_CALLBACK (add_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "remove_clicked",
		G_CALLBACK (remove_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "add_row_activated",
		G_CALLBACK (add_row_activated), swidget);
	glade_xml_signal_connect_data (swidget->xml, "remove_row_activated",
		G_CALLBACK (remove_row_activated), swidget);
	
	/* Initialize key stores */
	all_keys = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, ALL));
	srecips->all_keys = seahorse_recipients_store_new (sctx, all_keys);
	seahorse_key_store_populate (srecips->all_keys);
	
	recips_keys = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, RECIPIENTS));
	srecips->recipient_keys = seahorse_recipients_store_new (sctx, recips_keys);
	
	recipients = glade_xml_get_widget (swidget->xml, swidget->name);
	gtk_widget_show_all (recipients);

	return swidget;
}

GpgmeRecipients
seahorse_recipients_run (SeahorseRecipients *srecips)
{	
	GtkWidget *recipients;
	SeahorseWidget *swidget;
	gint response;
	gboolean done = FALSE;

	swidget = SEAHORSE_WIDGET (swidget);
	recipients = glade_xml_get_widget (swidget->xml, swidget->name);
	
	while (!done) {
		response = gtk_dialog_run (GTK_DIALOG (recipients));
		
		switch (response) {
			case GTK_RESPONSE_HELP:
				break;
			case GTK_RESPONSE_OK:
				done = TRUE;
				break;
			default:
				gpgme_recipients_release (srecips->recips);
				srecips->recips = NULL;
				done = TRUE;
				break;
		}
	}
	
	return srecips->recips;
}
