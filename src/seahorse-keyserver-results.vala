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

using GConf;
using Gtk;
using GLib;

namespace Seahorse {
	public class KeyserverResults : Viewer, View {
	
		private string _search_string;
		private Gtk.TreeView _view;
		private Gtk.ActionGroup _key_actions;
		private KeyManagerStore _store;
		private Set _objects; 
		private Object.Predicate _pred;
		
		private static const Gtk.ActionEntry[] GENERAL_ENTRIES = {
    			{ "remote-menu", null, N_("_Remote") },

    			{ "app-close", Gtk.STOCK_CLOSE, N_("_Close"), "<control>W",
    			  	N_("Close this window"), null }, 
    			{ "view-expand-all", Gtk.STOCK_ADD, N_("_Expand All"), null,
    		            	N_("Expand all listings"), null }, 
    		        { "view-collapse-all", Gtk.STOCK_REMOVE, N_("_Collapse All"), null,
    		          	N_("Collapse all listings"), null }
    		};
		
		private static const Gtk.ActionEntry[] SERVER_ENTRIES = {
			{ "remote-find", Gtk.STOCK_FIND, N_("_Find Remote Keys..."), "",
				N_("Search for keys on a key server"), null } 
		}; 
		
		private static const Gtk.ActionEntry[] KEY_ENTRIES = {
		        { "key-import-keyring", Gtk.STOCK_ADD, N_("_Import"), "",
		          	N_("Import selected keys to local key ring"), null }
		};
		
		KeyserverResults(string search_text) {
			this.name = "keyserver-results";
			this.search = search_text;
		}
		
		construct {
			string title;
			if (_search_string.len() == 0)
				title = _("Remote Keys");
			else 
				title = _("Remote Keys Containing '%s'").printf(_search_string);
			window.set_title(title);

			/* 
			 * We hook callbacks up here for now because of a compiler warning. See:
			 * http://bugzilla.gnome.org/show_bug.cgi?id=539483
			 */
			 
			var actions = new Gtk.ActionGroup ("general");
			actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			actions.add_actions (GENERAL_ENTRIES, this);
			actions.get_action ("app-close").activate += on_app_close;
			actions.get_action ("view-expand-all").activate += on_view_expand_all;
			actions.get_action ("view-collapse-all").activate += on_view_collapse_all;
			include_actions (actions);
			
			actions = new Gtk.ActionGroup ("keyserver");
			actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			actions.add_actions (SERVER_ENTRIES, this);
			actions.get_action ("remote-find").activate += on_remote_find;
			include_actions (actions);
			
			_key_actions = new Gtk.ActionGroup("key");
			_key_actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			_key_actions.add_actions (KEY_ENTRIES, this);
			_key_actions.get_action ("key-import-keyring").activate += on_key_import_keyring;
			_key_actions.get_action ("key-import-keyring").is_important = true;
			include_actions (_key_actions);


			/* init key list & selection settings */
			_view = (Gtk.TreeView)get_widget ("key_list");
			_view.get_selection().set_mode (Gtk.SelectionMode.MULTIPLE);
			_view.get_selection().changed += on_view_selection_changed;
			_view.row_activated += on_row_activated;
			_view.button_press_event += on_key_list_button_pressed;
			_view.popup_menu += on_key_list_popup_menu;
			_view.realize();
			
			/* Set focus to the current key list */
			_view.grab_focus ();

			/* To avoid flicker */
			ensure_updated ();
			base.show ();

			/* Our predicate for filtering keys */
			_pred.tag = Quark.from_string ("openpgp");
			_pred.usage = Usage.PUBLIC_KEY;
			_pred.location = Location.REMOTE;
			_pred.custom = on_filter_objects;
			  
			/* Our set all nicely filtered */
			_objects = new Set.full (ref _pred);
			_store = new KeyManagerStore (_objects, _view);

			on_view_selection_changed (_view.get_selection ());
		}
		
		public static void show (Operation op, Gtk.Window? parent, string search_text) {
			KeyserverResults res = new KeyserverResults (search_text);
			res.ref(); /* Destorys itself with destroy */
			if (parent != null) {
				Gtk.Window window = (Gtk.Window)res.get_toplevel();
				window.set_transient_for (parent);
			}
			Progress.status_set_operation (res, op);
		}
		
		public string search {
			get { return _search_string; }
			construct {
				/* Many key servers ignore spaces at the beginning and end, so we do too */
				string str = value;
				_search_string = str.strip().casefold(); 
			}
		}
		
		public override List<weak Object> get_selected_objects () {
			return KeyManagerStore.get_selected_keys (_view);
		}
		
		public override void set_selected_objects (List<Object> keys) {
			KeyManagerStore.set_selected_keys (_view, keys);
		}
		
		public override weak Object? selected {
			get { return KeyManagerStore.get_selected_key (_view, null); }
			set { 
				List<weak Object> keys = new List<weak Object>();
				if (value != null)
					keys.prepend (value);
				set_selected_objects (keys);
			}
		}
		
		public override weak Object? get_selected_object_and_uid (out uint uid) {
			return KeyManagerStore.get_selected_key (_view, out uid);
		}

		private bool on_filter_objects (Object obj) {
			if (_search_string.len() == 0)
				return true;
			string name = ((Key)obj).display_name;
			return (name.casefold().str(_search_string) != null); 
		}

		private void on_view_selection_changed (Gtk.TreeSelection selection)
		{
			GLib.Idle.add (fire_selection_changed);
		}
		
		private void on_row_activated (Gtk.TreeView view, Gtk.TreePath path, 
		                               Gtk.TreeViewColumn column) {
			Key key = KeyManagerStore.get_key_from_path (view, path, null);
			if (key != null)
				show_properties (key);
		}

		private bool on_key_list_button_pressed (Gtk.Widget widget, Gdk.EventButton event) {
			if (event.button == 3)
				show_context_menu (event.button, event.time);
			return false;
		}
		
		private bool on_key_list_popup_menu (Gtk.TreeView view) {
			var key = this.selected;
			if (key == null)
				return false;
			show_context_menu (0, Gtk.get_current_event_time ());
			return true;
		} 

		private void on_view_expand_all (Gtk.Action action) {
			_view.expand_all ();
		}
		
		private void on_view_collapse_all (Gtk.Action action) {
			_view.collapse_all ();
		}
		
		private void on_app_close (Gtk.Action? action) {
			this.destroy ();
		}
		
		private void imported_keys (Operation op)
		{
			if (!op.is_successful ()) {
				op.display_error (_("Couldn't import keys"), window);
				return;
			}

			set_status (_("Imported keys"));
		}
		
		private void on_key_import_keyring (Action action) {
			List<weak Object> keys = get_selected_objects();

			/* No keys, nothing to do */
			if (keys == null)
				return;

			Operation op = Context.for_app().transfer_objects (keys, null);
			Progress.show (op, _("Importing keys from key servers"), true);
			op.watch (imported_keys, null);
		}
		
		private void on_remote_find (Action action) {
			KeyserverSearch.show (window);
		}
		
		/* When this window closes we quit seahorse */	
		private bool on_delete_event (Gtk.Widget widget, Gdk.Event event) {
			on_app_close (null);
			return true;
		}

		private bool fire_selection_changed () {

			int rows = 0;
			
			Gtk.TreeSelection selection = _view.get_selection();
			rows = selection.count_selected_rows ();

			set_numbered_status (Bugs.ngettext ("Selected %d key", 
			                                    "Selected %d keys", rows), rows);

			_key_actions.set_sensitive (rows > 0);
			this.selection_changed();
			
			return false;
		}
	}
}
