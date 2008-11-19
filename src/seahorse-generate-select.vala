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

using Gtk;
using GLib;

namespace Seahorse {
	public class GenerateSelect : Widget {

		private Gtk.ListStore _store;
		private Gtk.TreeView _view;
		private Gtk.Dialog _dialog;
		private Generator[] _generators;
		
		private static const string TEMPLATE = 
			"<span size=\"larger\" weight=\"bold\">%s</span>\n%s";
		
		enum Column {
			ICON,
			TEXT,
			ACTION,
			N_COLUMNS
		}
		
		GenerateSelect () {
			/* The glade name */
			this.name = "generate-select";
		}
		
		construct {
			_store = new Gtk.ListStore (Column.N_COLUMNS, typeof (string), 
			                            typeof (string), typeof (Gtk.Action));
		
			var types = Registry.get().find_types ("generator", null);
			_generators = new Generator[types.length()];
			
			int i = 0;
			foreach (var typ in types) {
				Generator generator = (Generator)GLib.Object.new (typ, null);
				_generators[i++] = generator;
				
				/* Add each action to our store */
				Gtk.ActionGroup? group = generator.actions;
				if (group != null) {
					weak GLib.List<Action> actions = group.list_actions ();
					foreach (Gtk.Action action in actions) {
						
						string text = TEMPLATE.printf(action.label, action.tooltip);
						string icon = action.stock_id;
						
						Gtk.TreeIter iter;
						_store.append (out iter);
						_store.set (iter, Column.TEXT, text, Column.ICON, icon, 
						            Column.ACTION, action, -1);
					}
				}
			}
			
			/* Hook it into the view */
			_view = (Gtk.TreeView)get_widget ("keytype-tree");
			
			Gtk.CellRendererPixbuf pixcell = new CellRendererPixbuf ();
			pixcell.stock_size = Gtk.IconSize.DIALOG;
			_view.insert_column_with_attributes (-1, "", pixcell, "stock-id", Column.ICON, null);
			_view.insert_column_with_attributes (-1, "", new CellRendererText (), "markup", Column.TEXT, null);
			_view.set_model (_store);

			/* Setup selection, select first item */
			_view.get_selection().set_mode (Gtk.SelectionMode.BROWSE);
			
			Gtk.TreeIter iter;
			_store.get_iter_first (out iter);
			_view.get_selection().select_iter (iter);
			
			_view.row_activated += on_row_activated;
			
			_dialog = (Gtk.Dialog)get_toplevel ();
			_dialog.response += on_response;
		}
		
		public static void show (Gtk.Window? parent) {
			GenerateSelect sel = new GenerateSelect ();
			sel.ref(); /* Destorys itself with destroy */
			if (parent != null)
				sel._dialog.set_transient_for (parent);
		}
		
		private void fire_selected_action ()
		{
			Gtk.TreeIter iter;
			Gtk.TreeModel model;
			
			if (!_view.get_selection().get_selected (out model, out iter))
				return;
			
			Gtk.Action? action;
			model.get (iter, Column.ACTION, out action, -1);
			assert (action != null);
			
			action.activate();
		}

		private void on_row_activated (Gtk.TreeView view, Gtk.TreePath path, Gtk.TreeViewColumn col) 
		{
			fire_selected_action ();
			base.destroy ();
		}
		
		private void on_response (Gtk.Dialog dialog, int response)
		{
			if (response == Gtk.ResponseType.OK)
				fire_selected_action ();
			base.destroy ();
		}
	}
}
