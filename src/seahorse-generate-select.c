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

#include "seahorse-generate-select.h"
#include <seahorse-generator.h>
#include <common/seahorse-registry.h>
#include <stdlib.h>
#include <string.h>


#define SEAHORSE_GENERATE_SELECT_TYPE_COLUMN (seahorse_generate_select_column_get_type ())

typedef enum  {
	SEAHORSE_GENERATE_SELECT_COLUMN_ICON,
	SEAHORSE_GENERATE_SELECT_COLUMN_TEXT,
	SEAHORSE_GENERATE_SELECT_COLUMN_ACTION,
	SEAHORSE_GENERATE_SELECT_COLUMN_N_COLUMNS
} SeahorseGenerateSelectColumn;



struct _SeahorseGenerateSelectPrivate {
	GtkListStore* _store;
	GtkTreeView* _view;
	GtkDialog* _dialog;
	SeahorseGenerator** _generators;
	gint _generators_length1;
};

#define SEAHORSE_GENERATE_SELECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_TYPE_GENERATE_SELECT, SeahorseGenerateSelectPrivate))
enum  {
	SEAHORSE_GENERATE_SELECT_DUMMY_PROPERTY
};
GType seahorse_generate_select_column_get_type (void);
static SeahorseGenerateSelect* seahorse_generate_select_new (void);
static void seahorse_generate_select_fire_selected_action (SeahorseGenerateSelect* self);
static void seahorse_generate_select_on_row_activated (SeahorseGenerateSelect* self, GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* col);
static void seahorse_generate_select_on_response (SeahorseGenerateSelect* self, GtkDialog* dialog, gint response);
static void _seahorse_generate_select_on_row_activated_gtk_tree_view_row_activated (GtkTreeView* _sender, GtkTreePath* path, GtkTreeViewColumn* column, gpointer self);
static void _seahorse_generate_select_on_response_gtk_dialog_response (GtkDialog* _sender, gint response_id, gpointer self);
static GObject * seahorse_generate_select_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_generate_select_parent_class = NULL;
static void seahorse_generate_select_finalize (GObject * obj);
static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func);

static const char* SEAHORSE_GENERATE_SELECT_TEMPLATE = "<span size=\"larger\" weight=\"bold\">%s</span>\n%s";



GType seahorse_generate_select_column_get_type (void) {
	static GType seahorse_generate_select_column_type_id = 0;
	if (G_UNLIKELY (seahorse_generate_select_column_type_id == 0)) {
		static const GEnumValue values[] = {{SEAHORSE_GENERATE_SELECT_COLUMN_ICON, "SEAHORSE_GENERATE_SELECT_COLUMN_ICON", "icon"}, {SEAHORSE_GENERATE_SELECT_COLUMN_TEXT, "SEAHORSE_GENERATE_SELECT_COLUMN_TEXT", "text"}, {SEAHORSE_GENERATE_SELECT_COLUMN_ACTION, "SEAHORSE_GENERATE_SELECT_COLUMN_ACTION", "action"}, {SEAHORSE_GENERATE_SELECT_COLUMN_N_COLUMNS, "SEAHORSE_GENERATE_SELECT_COLUMN_N_COLUMNS", "n-columns"}, {0, NULL, NULL}};
		seahorse_generate_select_column_type_id = g_enum_register_static ("SeahorseGenerateSelectColumn", values);
	}
	return seahorse_generate_select_column_type_id;
}


static SeahorseGenerateSelect* seahorse_generate_select_new (void) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorseGenerateSelect * self;
	__params = g_new0 (GParameter, 1);
	__params_it = __params;
	__params_it->name = "name";
	g_value_init (&__params_it->value, G_TYPE_STRING);
	g_value_set_string (&__params_it->value, "generate-select");
	__params_it++;
	self = g_object_newv (SEAHORSE_TYPE_GENERATE_SELECT, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


void seahorse_generate_select_show (GtkWindow* parent) {
	SeahorseGenerateSelect* sel;
	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
	sel = g_object_ref_sink (seahorse_generate_select_new ());
	g_object_ref (G_OBJECT (sel));
	/* Destorys itself with destroy */
	if (parent != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW (sel->priv->_dialog), parent);
	}
	(sel == NULL ? NULL : (sel = (g_object_unref (sel), NULL)));
}


static void seahorse_generate_select_fire_selected_action (SeahorseGenerateSelect* self) {
	GtkTreeIter iter = {0};
	GtkTreeModel* model;
	GtkTreeModel* _tmp3;
	GtkTreeModel* _tmp2;
	gboolean _tmp1;
	GtkTreeModel* _tmp0;
	GtkAction* action;
	g_return_if_fail (SEAHORSE_IS_GENERATE_SELECT (self));
	model = NULL;
	_tmp3 = NULL;
	_tmp2 = NULL;
	_tmp0 = NULL;
	if (!(_tmp1 = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->priv->_view), &_tmp0, &iter), model = (_tmp2 = (_tmp3 = _tmp0, (_tmp3 == NULL ? NULL : g_object_ref (_tmp3))), (model == NULL ? NULL : (model = (g_object_unref (model), NULL))), _tmp2), _tmp1)) {
		(model == NULL ? NULL : (model = (g_object_unref (model), NULL)));
		return;
	}
	action = NULL;
	gtk_tree_model_get (model, &iter, SEAHORSE_GENERATE_SELECT_COLUMN_ACTION, &action, -1, -1);
	g_assert (action != NULL);
	gtk_action_activate (action);
	(model == NULL ? NULL : (model = (g_object_unref (model), NULL)));
	(action == NULL ? NULL : (action = (g_object_unref (action), NULL)));
}


static void seahorse_generate_select_on_row_activated (SeahorseGenerateSelect* self, GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* col) {
	g_return_if_fail (SEAHORSE_IS_GENERATE_SELECT (self));
	g_return_if_fail (GTK_IS_TREE_VIEW (view));
	g_return_if_fail (path != NULL);
	g_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (col));
	seahorse_generate_select_fire_selected_action (self);
	seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}


static void seahorse_generate_select_on_response (SeahorseGenerateSelect* self, GtkDialog* dialog, gint response) {
	g_return_if_fail (SEAHORSE_IS_GENERATE_SELECT (self));
	g_return_if_fail (GTK_IS_DIALOG (dialog));
	if (response == GTK_RESPONSE_OK) {
		seahorse_generate_select_fire_selected_action (self);
	}
	seahorse_widget_destroy (SEAHORSE_WIDGET (self));
}


static void _seahorse_generate_select_on_row_activated_gtk_tree_view_row_activated (GtkTreeView* _sender, GtkTreePath* path, GtkTreeViewColumn* column, gpointer self) {
	seahorse_generate_select_on_row_activated (self, _sender, path, column);
}


static void _seahorse_generate_select_on_response_gtk_dialog_response (GtkDialog* _sender, gint response_id, gpointer self) {
	seahorse_generate_select_on_response (self, _sender, response_id);
}


static GObject * seahorse_generate_select_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorseGenerateSelectClass * klass;
	GObjectClass * parent_class;
	SeahorseGenerateSelect * self;
	klass = SEAHORSE_GENERATE_SELECT_CLASS (g_type_class_peek (SEAHORSE_TYPE_GENERATE_SELECT));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_GENERATE_SELECT (obj);
	{
		GtkListStore* _tmp0;
		GList* types;
		SeahorseGenerator** _tmp2;
		gint _tmp1;
		gint i;
		GtkTreeView* _tmp13;
		GtkTreeView* _tmp12;
		GtkCellRendererPixbuf* pixcell;
		guint _tmp14;
		GtkCellRendererText* _tmp15;
		GtkTreeIter iter = {0};
		GtkDialog* _tmp17;
		GtkDialog* _tmp16;
		_tmp0 = NULL;
		self->priv->_store = (_tmp0 = gtk_list_store_new (((gint) (SEAHORSE_GENERATE_SELECT_COLUMN_N_COLUMNS)), G_TYPE_STRING, G_TYPE_STRING, GTK_TYPE_ACTION, NULL), (self->priv->_store == NULL ? NULL : (self->priv->_store = (g_object_unref (self->priv->_store), NULL))), _tmp0);
		types = seahorse_registry_find_types (seahorse_registry_get (), "generator", NULL, NULL);
		_tmp2 = NULL;
		self->priv->_generators = (_tmp2 = g_new0 (SeahorseGenerator*, (_tmp1 = g_list_length (types)) + 1), (self->priv->_generators = (_vala_array_free (self->priv->_generators, self->priv->_generators_length1, ((GDestroyNotify) (g_object_unref))), NULL)), self->priv->_generators_length1 = _tmp1, _tmp2);
		i = 0;
		{
			GList* typ_collection;
			GList* typ_it;
			typ_collection = types;
			for (typ_it = typ_collection; typ_it != NULL; typ_it = typ_it->next) {
				GType typ;
				typ = GPOINTER_TO_INT (typ_it->data);
				{
					SeahorseGenerator* generator;
					gint _tmp5;
					SeahorseGenerator* _tmp4;
					SeahorseGenerator* _tmp3;
					GtkActionGroup* _tmp6;
					GtkActionGroup* group;
					generator = SEAHORSE_GENERATOR (g_object_new (typ, NULL, NULL));
					_tmp4 = NULL;
					_tmp3 = NULL;
					_tmp5 = i++;
					self->priv->_generators[_tmp5] = (_tmp4 = (_tmp3 = generator, (_tmp3 == NULL ? NULL : g_object_ref (_tmp3))), (self->priv->_generators[_tmp5] == NULL ? NULL : (self->priv->_generators[_tmp5] = (g_object_unref (self->priv->_generators[_tmp5]), NULL))), _tmp4);
					/* Add each action to our store */
					_tmp6 = NULL;
					group = (_tmp6 = seahorse_generator_get_actions (generator), (_tmp6 == NULL ? NULL : g_object_ref (_tmp6)));
					if (group != NULL) {
						GList* actions;
						actions = gtk_action_group_list_actions (group);
						{
							GList* action_collection;
							GList* action_it;
							action_collection = actions;
							for (action_it = action_collection; action_it != NULL; action_it = action_it->next) {
								GtkAction* _tmp11;
								GtkAction* action;
								_tmp11 = NULL;
								action = (_tmp11 = ((GtkAction*) (action_it->data)), (_tmp11 == NULL ? NULL : g_object_ref (_tmp11)));
								{
									char* _tmp8;
									char* _tmp7;
									char* text;
									const char* _tmp10;
									char* _tmp9;
									char* icon;
									GtkTreeIter iter = {0};
									_tmp8 = NULL;
									_tmp7 = NULL;
									text = g_strdup_printf (SEAHORSE_GENERATE_SELECT_TEMPLATE, (g_object_get (G_OBJECT (action), "label", &_tmp7, NULL), _tmp7), (g_object_get (G_OBJECT (action), "tooltip", &_tmp8, NULL), _tmp8));
									_tmp10 = NULL;
									_tmp9 = NULL;
									icon = (_tmp10 = (g_object_get (G_OBJECT (action), "stock-id", &_tmp9, NULL), _tmp9), (_tmp10 == NULL ? NULL : g_strdup (_tmp10)));
									gtk_list_store_append (self->priv->_store, &iter);
									gtk_list_store_set (self->priv->_store, &iter, SEAHORSE_GENERATE_SELECT_COLUMN_TEXT, text, SEAHORSE_GENERATE_SELECT_COLUMN_ICON, icon, SEAHORSE_GENERATE_SELECT_COLUMN_ACTION, action, -1, -1);
									(action == NULL ? NULL : (action = (g_object_unref (action), NULL)));
									text = (g_free (text), NULL);
									icon = (g_free (icon), NULL);
								}
							}
						}
					}
					(generator == NULL ? NULL : (generator = (g_object_unref (generator), NULL)));
					(group == NULL ? NULL : (group = (g_object_unref (group), NULL)));
				}
			}
		}
		/* Hook it into the view */
		_tmp13 = NULL;
		_tmp12 = NULL;
		self->priv->_view = (_tmp13 = (_tmp12 = GTK_TREE_VIEW (seahorse_widget_get_widget (SEAHORSE_WIDGET (self), "keytype-tree")), (_tmp12 == NULL ? NULL : g_object_ref (_tmp12))), (self->priv->_view == NULL ? NULL : (self->priv->_view = (g_object_unref (self->priv->_view), NULL))), _tmp13);
		pixcell = g_object_ref_sink (((GtkCellRendererPixbuf*) (gtk_cell_renderer_pixbuf_new ())));
		g_object_set (pixcell, "stock-size", ((guint) (GTK_ICON_SIZE_DIALOG)), NULL);
		gtk_tree_view_insert_column_with_attributes (self->priv->_view, -1, "", GTK_CELL_RENDERER (pixcell), "stock-id", SEAHORSE_GENERATE_SELECT_COLUMN_ICON, NULL, NULL);
		_tmp15 = NULL;
		gtk_tree_view_insert_column_with_attributes (self->priv->_view, -1, "", GTK_CELL_RENDERER ((_tmp15 = g_object_ref_sink (((GtkCellRendererText*) (gtk_cell_renderer_text_new ()))))), "markup", SEAHORSE_GENERATE_SELECT_COLUMN_TEXT, NULL, NULL);
		(_tmp15 == NULL ? NULL : (_tmp15 = (g_object_unref (_tmp15), NULL)));
		gtk_tree_view_set_model (self->priv->_view, GTK_TREE_MODEL (self->priv->_store));
		/* Setup selection, select first item */
		gtk_tree_selection_set_mode (gtk_tree_view_get_selection (self->priv->_view), GTK_SELECTION_BROWSE);
		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->_store), &iter);
		gtk_tree_selection_select_iter (gtk_tree_view_get_selection (self->priv->_view), &iter);
		g_signal_connect_object (self->priv->_view, "row-activated", ((GCallback) (_seahorse_generate_select_on_row_activated_gtk_tree_view_row_activated)), self, 0);
		_tmp17 = NULL;
		_tmp16 = NULL;
		self->priv->_dialog = (_tmp17 = (_tmp16 = GTK_DIALOG (seahorse_widget_get_toplevel (SEAHORSE_WIDGET (self))), (_tmp16 == NULL ? NULL : g_object_ref (_tmp16))), (self->priv->_dialog == NULL ? NULL : (self->priv->_dialog = (g_object_unref (self->priv->_dialog), NULL))), _tmp17);
		g_signal_connect_object (self->priv->_dialog, "response", ((GCallback) (_seahorse_generate_select_on_response_gtk_dialog_response)), self, 0);
		(types == NULL ? NULL : (types = (g_list_free (types), NULL)));
		(pixcell == NULL ? NULL : (pixcell = (g_object_unref (pixcell), NULL)));
	}
	return obj;
}


static void seahorse_generate_select_class_init (SeahorseGenerateSelectClass * klass) {
	seahorse_generate_select_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGenerateSelectPrivate));
	G_OBJECT_CLASS (klass)->constructor = seahorse_generate_select_constructor;
	G_OBJECT_CLASS (klass)->finalize = seahorse_generate_select_finalize;
}


static void seahorse_generate_select_instance_init (SeahorseGenerateSelect * self) {
	self->priv = SEAHORSE_GENERATE_SELECT_GET_PRIVATE (self);
}


static void seahorse_generate_select_finalize (GObject * obj) {
	SeahorseGenerateSelect * self;
	self = SEAHORSE_GENERATE_SELECT (obj);
	(self->priv->_store == NULL ? NULL : (self->priv->_store = (g_object_unref (self->priv->_store), NULL)));
	(self->priv->_view == NULL ? NULL : (self->priv->_view = (g_object_unref (self->priv->_view), NULL)));
	(self->priv->_dialog == NULL ? NULL : (self->priv->_dialog = (g_object_unref (self->priv->_dialog), NULL)));
	self->priv->_generators = (_vala_array_free (self->priv->_generators, self->priv->_generators_length1, ((GDestroyNotify) (g_object_unref))), NULL);
	G_OBJECT_CLASS (seahorse_generate_select_parent_class)->finalize (obj);
}


GType seahorse_generate_select_get_type (void) {
	static GType seahorse_generate_select_type_id = 0;
	if (seahorse_generate_select_type_id == 0) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseGenerateSelectClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_generate_select_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseGenerateSelect), 0, (GInstanceInitFunc) seahorse_generate_select_instance_init };
		seahorse_generate_select_type_id = g_type_register_static (SEAHORSE_TYPE_WIDGET, "SeahorseGenerateSelect", &g_define_type_info, 0);
	}
	return seahorse_generate_select_type_id;
}


static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func) {
	if (array != NULL && destroy_func != NULL) {
		int i;
		if (array_length >= 0)
		for (i = 0; i < array_length; i = i + 1) {
			if (((gpointer*) (array))[i] != NULL)
			destroy_func (((gpointer*) (array))[i]);
		}
		else
		for (i = 0; ((gpointer*) (array))[i] != NULL; i = i + 1) {
			destroy_func (((gpointer*) (array))[i]);
		}
	}
	g_free (array);
}




