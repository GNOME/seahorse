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
#include <eel/eel.h>

#include "seahorse-recipients.h"
#include "seahorse-widget.h"
#include "seahorse-ops-key.h"
#include "seahorse-validity.h"
#include "seahorse-encrypt-recipients-store.h"
#include "seahorse-preferences.h"

#define ALL "all_keys"
#define RECIPS "recipient_keys"

enum {
	PROP_0,
	PROP_VALIDITY
};

static void	seahorse_recipients_class_init		(SeahorseRecipientsClass	*klass);
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
			0, NULL
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
		g_param_spec_boolean ("validity", "If need validity",
				      "TRUE if recipients need to have full validity",
				      FALSE, G_PARAM_READWRITE));
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

static void
set_all_buttons (SeahorseWidget *swidget)
{
	SeahorseRecipients *srecips;
	GtkTreeIter iter;
	gboolean sensitive;
	
	srecips = SEAHORSE_RECIPIENTS (swidget);
	
	sensitive = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (
		srecips->recipient_keys), &iter);
	gtk_widget_set_sensitive (glade_xml_get_widget (
		swidget->xml, "ok"), sensitive);
}

static gboolean
add_recip (SeahorseWidget *swidget, SeahorseKey *skey)
{
	SeahorseValidity validity;
	GtkWidget *dialog;
	gint response;
	
	/* if don't need validity check */
	if (!SEAHORSE_RECIPIENTS (swidget)->need_validity)
		return TRUE;
	
	validity = seahorse_key_get_validity (skey);
	/* if is already fully valid */
	if (validity >= SEAHORSE_VALIDITY_FULL)
		return TRUE;
	
	dialog = gtk_message_dialog_new (GTK_WINDOW (glade_xml_get_widget (
		swidget->xml, swidget->name)), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO, _("%s is not a fully valid key."
		" Would you like to temporarily give it full validity?"),
		seahorse_key_get_userid (skey, 0));
		
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	/* if answer yes */
	return (response == GTK_RESPONSE_YES);
}

static gboolean
add_all_recips (SeahorseWidget *swidget, gint length)
{
	GtkWidget *dialog;
	gint response;
	
	g_return_val_if_fail (SEAHORSE_RECIPIENTS (swidget)->need_validity, FALSE);
	g_return_val_if_fail (length > eel_gconf_get_integer (VALIDITY_THRESHOLD), FALSE);
	
	dialog = gtk_message_dialog_new (GTK_WINDOW (glade_xml_get_widget (
		swidget->xml, swidget->name)), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO, _("You are about to add %d keys to the recipients list."
		" Some of them may not be fully valid. Would you like to temporarily"
		" give all %d keys full validity?"), length, length);
	
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	
	return (response == GTK_RESPONSE_YES);
}

/* Add recipient button clicked */
static void
add_clicked (GtkButton *button, SeahorseRecipients *srecips)
{	
	GList *selected, *list;
	SeahorseKey *skey;
	GtkTreeView *view;
	SeahorseWidget *swidget;
	gboolean check_valid = FALSE;
	gint length;
	
	swidget = SEAHORSE_WIDGET (srecips);
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, ALL));
	selected = list = gtk_tree_selection_get_selected_rows (
		gtk_tree_view_get_selection (view), NULL);
	length = g_list_length (list);
	
	check_valid = (srecips->need_validity && length > eel_gconf_get_integer (VALIDITY_THRESHOLD));
	/* if above threshold but don't want to add */
	if (check_valid && !add_all_recips (swidget, length)) {
		g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (list);
		return;
	}
	
	/* add list to rhs */
	while (list != NULL) {
		skey = seahorse_key_store_get_key_from_path (view, list->data);
		if (!srecips->need_validity || check_valid || add_recip (swidget, skey))
			seahorse_key_store_append (srecips->recipient_keys, skey);
		else {
			gtk_tree_path_free (list->data);
			list->data = NULL;
		}
		list = g_list_next (list);
	}
	/* remove list from lhs */
	list = g_list_last (selected);
	while (list != NULL) {
		if (list->data != NULL) {
			seahorse_key_store_remove (srecips->all_keys, list->data);
			gtk_tree_path_free (list->data);
		}
		list = g_list_previous (list);
	}
	g_list_free (selected);
	
	set_all_buttons (swidget);
}

/* Remove recipient button clicked */
static void
remove_clicked (GtkButton *button, SeahorseRecipients *srecips)
{
	GList *selected, *list;
	GtkTreeView *view;
	SeahorseWidget *swidget;
	
	swidget = SEAHORSE_WIDGET (srecips);
	view = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, RECIPS));
	selected = list = gtk_tree_selection_get_selected_rows (
		gtk_tree_view_get_selection (view), NULL);
	
	/* add list to rhs */
	while (list != NULL) {
		seahorse_key_store_append (srecips->all_keys, seahorse_key_store_get_key_from_path (
			view, list->data));
		list = g_list_next (list);
	}
	/* remove list from lhs */
	list = g_list_last (selected);
	while (list != NULL) {
		seahorse_key_store_remove (srecips->recipient_keys, list->data);
		gtk_tree_path_free (list->data);
		list = g_list_previous (list);
	}
	g_list_free (selected);
	
	set_all_buttons (swidget);
}

static void
transfer (SeahorseWidget *swidget, SeahorseKeyStore *lhs, SeahorseKeyStore *rhs,
	  SeahorseKey *skey, GtkTreePath *path)
{
	seahorse_key_store_remove (lhs, path);
	seahorse_key_store_append (rhs, skey);
	set_all_buttons (swidget);
}

/* Right (non-recip) key activated */
static void
add_row_activated (GtkTreeView *view, GtkTreePath *path,
		   GtkTreeViewColumn *arg2, SeahorseRecipients *srecips)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_key_from_model (GTK_TREE_MODEL (
		srecips->all_keys), path);
	if (add_recip (SEAHORSE_WIDGET (srecips), skey))
		transfer (SEAHORSE_WIDGET (srecips), srecips->all_keys,
			srecips->recipient_keys, skey, path);
}

/* Left (recip) key activated */
static void
remove_row_activated (GtkTreeView *view, GtkTreePath *path,
		      GtkTreeViewColumn *arg2, SeahorseRecipients *srecips)
{
	SeahorseKey *skey;
	
	skey = seahorse_key_store_get_key_from_model (GTK_TREE_MODEL (
		srecips->recipient_keys), path);
	transfer (SEAHORSE_WIDGET (srecips), srecips->recipient_keys,
		srecips->all_keys, skey, path);
}

static void
all_selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	gint count;
	GtkStatusbar *status;
	guint id;
	
	count = gtk_tree_selection_count_selected_rows (selection);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "add"),
		count > 0);
	status = GTK_STATUSBAR (glade_xml_get_widget (swidget->xml, "all_status"));
	id = gtk_statusbar_get_context_id (status, "selection");
	gtk_statusbar_pop (status, id);
	gtk_statusbar_push (status, id, g_strdup_printf (_("Selected %d keys"), count));	
}

static void
recips_selection_changed (GtkTreeSelection *selection, SeahorseWidget *swidget)
{
	gint count;
	GtkStatusbar *status;
	guint id;
	
	count = gtk_tree_selection_count_selected_rows (selection);
	gtk_widget_set_sensitive (glade_xml_get_widget (swidget->xml, "remove"),
		count > 0);
	status = GTK_STATUSBAR (glade_xml_get_widget (swidget->xml, "recips_status"));
	id = gtk_statusbar_get_context_id (status, "selection");
	gtk_statusbar_pop (status, id);
	gtk_statusbar_push (status, id, g_strdup_printf (_("Selected %d keys"), count));	
}

/* Creates a new #SeahorseRecipients given @sctx and whether doing
 * encrypt recipients.
 */
static SeahorseWidget*
seahorse_recipients_new (SeahorseContext *sctx, gboolean use_encrypt)
{
	SeahorseRecipients *srecips;
	SeahorseWidget *swidget;
	GtkTreeView *all_keys;
	GtkTreeView *recips_keys;
	GtkTreeSelection *selection;
	
	srecips = g_object_new (SEAHORSE_TYPE_RECIPIENTS, "name", "recipients", "ctx", sctx,
		"validity", use_encrypt, NULL);
	swidget = SEAHORSE_WIDGET (srecips);
	
	glade_xml_signal_connect_data (swidget->xml, "add_clicked",
		G_CALLBACK (add_clicked), srecips);
	glade_xml_signal_connect_data (swidget->xml, "remove_clicked",
		G_CALLBACK (remove_clicked), srecips);
	glade_xml_signal_connect_data (swidget->xml, "add_row_activated",
		G_CALLBACK (add_row_activated), srecips);
	glade_xml_signal_connect_data (swidget->xml, "remove_row_activated",
		G_CALLBACK (remove_row_activated), srecips);
	
	all_keys = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, ALL));
	recips_keys = GTK_TREE_VIEW (glade_xml_get_widget (swidget->xml, RECIPS));

	if (use_encrypt) {
		srecips->all_keys = seahorse_encrypt_recipients_store_new (sctx, all_keys);
		srecips->recipient_keys = seahorse_encrypt_recipients_store_new (sctx, recips_keys);
	}
	else {
		srecips->all_keys = seahorse_recipients_store_new (sctx, all_keys);
		srecips->recipient_keys = seahorse_recipients_store_new (sctx, recips_keys);
	}

	seahorse_key_store_populate (srecips->all_keys);
	gtk_widget_show_all (glade_xml_get_widget (swidget->xml, swidget->name));
	/* listen to selection change */
	selection = gtk_tree_view_get_selection (all_keys);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_after (selection, "changed",
		G_CALLBACK (all_selection_changed), swidget);
	selection = gtk_tree_view_get_selection (recips_keys);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect_after (selection, "changed",
		G_CALLBACK (recips_selection_changed), swidget);

	return swidget;
}

/**
 * seahorse_export_recipients_new:
 * @sctx: Current #SeahorseContext
 *
 * Creates a new #SeahorseRecipients for exporting keys.
 *
 * Returns: The new #SeahorseWidget
 **/
SeahorseWidget*
seahorse_export_recipients_new (SeahorseContext *sctx)
{
	return seahorse_recipients_new (sctx, FALSE);
}

/**
 * seahorse_encrypt_recipients_new:
 * @sctx: Current #SeahorseContext
 *
 * Creates a new #SeahorseRecipients for encrypting data with keys.
 *
 * Returns: The new #SeahorseWidget
 **/
SeahorseWidget*
seahorse_encrypt_recipients_new (SeahorseContext *sctx)
{
	return seahorse_recipients_new (sctx, TRUE);
}

static gboolean
recipients_foreach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, GpgmeRecipients recips)
{
	seahorse_ops_key_recips_add (recips, seahorse_key_store_get_key_from_model (model, path));
	return FALSE;
}

/**
 * seahorse_recipients_run:
 * @srecips: #SeahorseRecipients to run
 *
 * Runs the #SeahorseRecipients dialog until user finishes.
 * @srecips must be destroyed after returning.
 *
 * Returns: The selected recipients, or NULL if none
 **/
GpgmeRecipients
seahorse_recipients_run (SeahorseRecipients *srecips)
{	
	SeahorseWidget *swidget;
	gint response;
	gboolean done = FALSE;
	GpgmeRecipients recips;

	swidget = SEAHORSE_WIDGET (srecips);
	g_return_val_if_fail (gpgme_recipients_new (&recips) == GPGME_No_Error, NULL);
	
	while (!done) {
		response = gtk_dialog_run (GTK_DIALOG (glade_xml_get_widget (
			swidget->xml, swidget->name)));
		
		switch (response) {
			case GTK_RESPONSE_HELP:
				break;
			case GTK_RESPONSE_OK:
				done = TRUE;
				gtk_tree_model_foreach (GTK_TREE_MODEL (srecips->recipient_keys),
					(GtkTreeModelForeachFunc)recipients_foreach, recips);
				if (gpgme_recipients_count (recips) == 0) {
					gpgme_recipients_release (recips);
					recips = NULL;
				}
				break;
			default:
				gpgme_recipients_release (recips);
				done = TRUE;
				break;
		}
	}
	
	return recips;
}
