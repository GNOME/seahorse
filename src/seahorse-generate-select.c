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

#include "seahorse-object-list.h"
#include "seahorse-registry.h"

typedef enum  {
	COLUMN_ICON,
	COLUMN_TEXT,
	COLUMN_ACTION,
	COLUMN_N_COLUMNS
} Column;

struct _SeahorseGenerateSelectPrivate {
	GtkListStore* store;
	GtkTreeView* view;
	GtkDialog* dialog;
	GList *action_groups;
};

G_DEFINE_TYPE (SeahorseGenerateSelect, seahorse_generate_select, SEAHORSE_TYPE_WIDGET);

static const char* TEMPLATE = "<span size=\"larger\" weight=\"bold\">%s</span>\n%s";

/* -----------------------------------------------------------------------------
 * INTERNAL 
 */

static gboolean 
fire_selected_action (SeahorseGenerateSelect* self) 
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkAction *action;
	
	selection = gtk_tree_view_get_selection (self->pv->view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (GTK_TREE_MODEL (self->pv->store), &iter,
	                    COLUMN_ACTION, &action, -1);
	g_assert (action != NULL);

	gtk_action_activate (action);
	return TRUE;
}

static void
on_row_activated (GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* col, SeahorseGenerateSelect* self) 
{
	g_return_if_fail (SEAHORSE_IS_GENERATE_SELECT (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (col));
	
	if (fire_selected_action (self))
		seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}

static void 
on_response (GtkDialog* dialog, gint response, SeahorseGenerateSelect* self) 
{
	g_return_if_fail (SEAHORSE_IS_GENERATE_SELECT (self));
	g_return_if_fail (GTK_IS_DIALOG (dialog));
	
	if (response == GTK_RESPONSE_OK) 
		fire_selected_action (self);

	seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */


static GObject* 
seahorse_generate_select_constructor (GType type, guint n_props, GObjectConstructParam *props) 
{
	SeahorseGenerateSelect *self = SEAHORSE_GENERATE_SELECT (G_OBJECT_CLASS (seahorse_generate_select_parent_class)->constructor(type, n_props, props));
	gchar *text;
	gchar *label, *tooltip;
	GtkCellRenderer *pixcell;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *l;
	GIcon *icon;

	g_return_val_if_fail (self, NULL);	

	self->pv->store = gtk_list_store_new (COLUMN_N_COLUMNS, G_TYPE_ICON, G_TYPE_STRING, GTK_TYPE_ACTION);

	self->pv->action_groups = seahorse_registry_object_instances (NULL, "generator", NULL);
	for (l = self->pv->action_groups; l; l = g_list_next (l)) {
		GList *k, *actions = gtk_action_group_list_actions (l->data);
		for (k = actions; k; k = g_list_next (k)) {
			
			g_object_get (k->data, "label", &label, "tooltip", &tooltip, "gicon", &icon, NULL);
			text = g_strdup_printf (TEMPLATE, label, tooltip);

			gtk_list_store_append (self->pv->store, &iter);
			gtk_list_store_set (self->pv->store, &iter, 
			                    COLUMN_TEXT, text, 
			                    COLUMN_ICON, icon, 
				            COLUMN_ACTION, k->data, 
				            -1);
			
			g_free (text);
			g_free (label);
			g_clear_object (&icon);
			g_free (tooltip);
		}
		
		g_list_free (actions);
	}
	
	/* Hook it into the view */
	self->pv->view = GTK_TREE_VIEW (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "keytype-tree"));
	g_return_val_if_fail (self->pv->view, NULL);
	
	pixcell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (pixcell, "stock-size", GTK_ICON_SIZE_DND, NULL);
	gtk_tree_view_insert_column_with_attributes (self->pv->view, -1, "", pixcell, "gicon", COLUMN_ICON, NULL);
	gtk_tree_view_insert_column_with_attributes (self->pv->view, -1, "", gtk_cell_renderer_text_new (), "markup", COLUMN_TEXT, NULL);
	gtk_tree_view_set_model (self->pv->view, GTK_TREE_MODEL (self->pv->store));

	/* Setup selection, select first item */
	selection = gtk_tree_view_get_selection (self->pv->view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->pv->store), &iter);
	gtk_tree_selection_select_iter (selection, &iter);

	g_signal_connect (self->pv->view, "row-activated", G_CALLBACK (on_row_activated), self);
	g_object_set (self->pv->view, "height-request", 410, NULL);

	self->pv->dialog = GTK_DIALOG (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self)));
	g_signal_connect (self->pv->dialog, "response", G_CALLBACK (on_response), self);
	
	return G_OBJECT (self);
}

static void
seahorse_generate_select_init (SeahorseGenerateSelect *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelectPrivate);
}

static void
seahorse_generate_select_finalize (GObject *obj)
{
	SeahorseGenerateSelect *self = SEAHORSE_GENERATE_SELECT (obj);

	if (self->pv->store != NULL)
		g_object_unref (self->pv->store);
	self->pv->store = NULL;

	seahorse_object_list_free (self->pv->action_groups);
	self->pv->action_groups = NULL;

	G_OBJECT_CLASS (seahorse_generate_select_parent_class)->finalize (obj);
}

static void
seahorse_generate_select_class_init (SeahorseGenerateSelectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_generate_select_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGenerateSelectPrivate));

	gobject_class->constructor = seahorse_generate_select_constructor;
	gobject_class->finalize = seahorse_generate_select_finalize;

}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

void 
seahorse_generate_select_show (GtkWindow* parent) 
{
	SeahorseGenerateSelect* sel;
	
	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
	
	sel = g_object_ref_sink (g_object_new (SEAHORSE_TYPE_GENERATE_SELECT, "name", "generate-select", NULL));
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (sel->pv->dialog), parent);
}
