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

#include "config.h"

#include "seahorse-generate-select.h"

#include "seahorse-common.h"

#include <glib/gi18n.h>

#define SEAHORSE_TYPE_GENERATE_SELECT             (seahorse_generate_select_get_type ())
#define SEAHORSE_GENERATE_SELECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelect))
#define SEAHORSE_GENERATE_SELECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelectClass))
#define SEAHORSE_IS_GENERATE_SELECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_GENERATE_SELECT))
#define SEAHORSE_IS_GENERATE_SELECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_GENERATE_SELECT))
#define SEAHORSE_GENERATE_SELECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelectClass))

GType              seahorse_generate_select_get_type   (void);

typedef struct _SeahorseGenerateSelect SeahorseGenerateSelect;
typedef struct _SeahorseGenerateSelectClass SeahorseGenerateSelectClass;

struct _SeahorseGenerateSelect {
	GtkDialog parent_instance;
	GtkListStore* store;
	GtkTreeView* view;
	GList *action_groups;
};

struct _SeahorseGenerateSelectClass {
	GtkDialogClass dialog_class;
};

typedef enum  {
	COLUMN_ICON,
	COLUMN_TEXT,
	COLUMN_ACTION,
	COLUMN_N_COLUMNS
} Column;

G_DEFINE_TYPE (SeahorseGenerateSelect, seahorse_generate_select, GTK_TYPE_DIALOG);

static const char* TEMPLATE = "<span size=\"larger\" weight=\"bold\">%s</span>\n%s";

static GtkAction *
get_selected_action (SeahorseGenerateSelect *self)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkAction *action;

	selection = gtk_tree_view_get_selection (self->view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
	                    COLUMN_ACTION, &action, -1);
	g_assert (action != NULL);

	return action;
}

static void
on_row_activated (GtkTreeView *view,
                  GtkTreePath *path,
                  GtkTreeViewColumn *col,
                  gpointer user_data)
{
	SeahorseGenerateSelect *self = SEAHORSE_GENERATE_SELECT (user_data);
	GtkAction *action;
	GtkWindow *parent;

	action = get_selected_action (self);
	if (action != NULL) {
		parent = gtk_window_get_transient_for (GTK_WINDOW (self));
		if (parent != NULL)
			g_object_ref (parent);

		g_object_ref (action);
		gtk_widget_destroy (GTK_WIDGET (self));

		seahorse_action_activate_with_window (action, NULL, parent);

		g_clear_object (&parent);
		g_object_unref (action);
	}
}

static void 
on_response (GtkDialog *dialog,
             gint response,
             gpointer user_data)
{
	SeahorseGenerateSelect *self = SEAHORSE_GENERATE_SELECT (user_data);
	GtkAction *action = NULL;
	GtkWindow *parent = NULL;

	if (response == GTK_RESPONSE_OK) 
		action = get_selected_action (self);
	if (action != NULL) {
		g_object_ref (action);
		parent = gtk_window_get_transient_for (GTK_WINDOW (self));
		if (parent != NULL)
			g_object_ref (parent);
	}

	gtk_widget_destroy (GTK_WIDGET (self));

	if (action != NULL) {
		seahorse_action_activate_with_window (action, NULL, parent);
		g_object_unref (action);
		g_clear_object (&parent);
	}
}

static gint
on_list_sort (GtkTreeModel *model,
              GtkTreeIter *a,
              GtkTreeIter *b,
              gpointer user_data)
{
	gchar *text_a;
	gchar *text_b;
	gint ret;

	gtk_tree_model_get (model, a, COLUMN_TEXT, &text_a, -1);
	gtk_tree_model_get (model, b, COLUMN_TEXT, &text_b, -1);

	ret = g_utf8_collate (text_a, text_b);

	g_free (text_a);
	g_free (text_b);

	return ret;
}

static void
seahorse_generate_select_constructed (GObject *obj)
{
	SeahorseGenerateSelect *self = SEAHORSE_GENERATE_SELECT (obj);
	gchar *text;
	GtkCellRenderer *pixcell;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *actions;
	GList *l, *k;
	GIcon *icon;
	GtkBuilder *builder;
	const gchar *path;
	GError *error = NULL;
	const gchar *icon_name;
	GtkAction *action;

	G_OBJECT_CLASS (seahorse_generate_select_parent_class)->constructed (obj);

	self->store = gtk_list_store_new (COLUMN_N_COLUMNS, G_TYPE_ICON, G_TYPE_STRING, GTK_TYPE_ACTION);
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (self->store), on_list_sort, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

	self->action_groups = seahorse_registry_object_instances ("generator");
	for (l = self->action_groups; l != NULL; l = g_list_next (l)) {
		actions = gtk_action_group_list_actions (l->data);
		for (k = actions; k != NULL; k = g_list_next (k)) {
			action = k->data;

			text = g_strdup_printf (TEMPLATE, gtk_action_get_label (action),
			                        gtk_action_get_tooltip (action));

			icon = gtk_action_get_gicon (action);
			if (icon == NULL) {
				icon_name = gtk_action_get_icon_name (action);
				if (icon_name)
					icon = g_themed_icon_new (icon_name);
				gtk_action_get_stock_id (action);
			} else {
				g_object_ref (icon);
			}

			gtk_list_store_append (self->store, &iter);
			gtk_list_store_set (self->store, &iter,
			                    COLUMN_TEXT, text, 
			                    COLUMN_ICON, icon, 
			                    COLUMN_ACTION, k->data,
			                    -1);
			g_clear_object (&icon);
		}

		g_list_free (actions);
	}

	builder = gtk_builder_new ();
	path = SEAHORSE_UIDIR "/seahorse-generate-select.xml";
	gtk_builder_add_from_file (builder, path, &error);
	if (error != NULL) {
		g_warning ("couldn't load ui file: %s", path);
		g_clear_error (&error);
		g_object_unref (builder);
		return;
	}

	/* Setup the dialog */
	gtk_window_set_modal (GTK_WINDOW (self), TRUE);
	gtk_window_set_default_size (GTK_WINDOW (self), -1, 410);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))),
	                    GTK_WIDGET (gtk_builder_get_object (builder, "generate-select")),
	                    TRUE, TRUE, 0);
	gtk_dialog_add_buttons (GTK_DIALOG (self),
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                        _("Continue"), GTK_RESPONSE_OK,
	                        NULL);

	/* Hook it into the view */
	self->view = GTK_TREE_VIEW (gtk_builder_get_object (builder, "keytype-tree"));

	g_object_unref (builder);

	pixcell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (pixcell, "stock-size", GTK_ICON_SIZE_DND, NULL);
	gtk_tree_view_insert_column_with_attributes (self->view, -1, "", pixcell, "gicon", COLUMN_ICON, NULL);
	gtk_tree_view_insert_column_with_attributes (self->view, -1, "", gtk_cell_renderer_text_new (), "markup", COLUMN_TEXT, NULL);
	gtk_tree_view_set_model (self->view, GTK_TREE_MODEL (self->store));

	/* Setup selection, select first item */
	selection = gtk_tree_view_get_selection (self->view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);
	gtk_tree_selection_select_iter (selection, &iter);

	g_signal_connect (self->view, "row-activated", G_CALLBACK (on_row_activated), self);
	g_object_set (self->view, "height-request", 410, NULL);

	g_signal_connect (self, "response", G_CALLBACK (on_response), self);
}

static void
seahorse_generate_select_init (SeahorseGenerateSelect *self)
{

}

static void
seahorse_generate_select_finalize (GObject *obj)
{
	SeahorseGenerateSelect *self = SEAHORSE_GENERATE_SELECT (obj);

	g_clear_object (&self->store);
	g_list_free_full (self->action_groups, g_object_unref);

	G_OBJECT_CLASS (seahorse_generate_select_parent_class)->finalize (obj);
}

static void
seahorse_generate_select_class_init (SeahorseGenerateSelectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = seahorse_generate_select_constructed;
	gobject_class->finalize = seahorse_generate_select_finalize;

}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

void 
seahorse_generate_select_show (GtkWindow* parent) 
{
	GtkWidget *dialog;

	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

	dialog = g_object_new (SEAHORSE_TYPE_GENERATE_SELECT,
	                       "transient-for", parent,
	                       NULL);

	gtk_widget_show (dialog);
}
