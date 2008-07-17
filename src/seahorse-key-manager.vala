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
	public class KeyManager : Viewer, View {
	
		private Gtk.Notebook _notebook;
		private Gtk.ActionGroup _view_actions;
		private Gtk.Entry _filter_entry;
		private Quark _track_selected_id;
		private uint _track_selected_tab;
		private bool _loaded_gnome_keyring;
		
		private enum Targets {
			PLAIN,
			URIS
		}
		
		/* 
		 * The various tabs. If adding a tab be sure to fix 
		 * logic throughout this file. 
		 */
		private static enum Tabs {
			PUBLIC = 0,
			TRUSTED,
			PRIVATE,
			PASSWORD,
			NUM_TABS
		}
		
		private struct TabInfo {
			public uint id;
			public int page;
			public weak Gtk.TreeView view;
			public weak Set objects;
			public weak Gtk.Widget widget;
			public KeyManagerStore store;
		}
		
		private TabInfo[] _tabs;

		private static const Object.Predicate PRED_PUBLIC = {
			0,                                      /* ktype */
			0,                                      /* id */
			Location.LOCAL,                         /* location */
			Usage.PUBLIC_KEY,                       /* usage */
			0,                                      /* flags */
			Key.Flag.TRUSTED | Key.Flag.IS_VALID,   /* nflags */
			null                                    /* sksrc */
		};
		
		private static const Object.Predicate PRED_TRUSTED = {
			0,                                      /* ktype */
			0,                                      /* id */
			Location.LOCAL,                         /* location */
			Usage.PUBLIC_KEY,                       /* usage */
			Key.Flag.TRUSTED | Key.Flag.IS_VALID,   /* flags */
			0,                                      /* nflags */
			null                                    /* sksrc */
		};
		
		private static const Object.Predicate PRED_PRIVATE = {
			0,                                      /* ktype */
			0,                                      /* id */
			Location.LOCAL,                         /* location */
			Usage.PRIVATE_KEY,                      /* usage */
			0,                                      /* flags */
			0,                                      /* nflags */
			null                                    /* sksrc */
		};

		private static const Object.Predicate PRED_PASSWORD = {
			0,                                      /* ktype */
		 	0,                                      /* id */
			Location.LOCAL,                         /* location */
			Usage.CREDENTIALS,                      /* usage */
			0,                                      /* flags */
			0,                                      /* nflags */
			null                                    /* sksrc */
		};
		
		private static const Gtk.ActionEntry[] GENERAL_ENTRIES = {
    			{ "remote-menu", null, N_("_Remote") },
    			
			{ "app-quit", Gtk.STOCK_QUIT, N_("_Quit"), "<control>Q",
				N_("Close this program"), null },

    			{ "key-generate", Gtk.STOCK_NEW, N_("_Create New Key..."), "<control>N", 
			  	N_("Create a new personal key"), null },
			{ "key-import-file", Gtk.STOCK_OPEN, N_("_Import..."), "<control>I",
				N_("Import keys into your key ring from a file"), null },
			{ "key-import-clipboard", Gtk.STOCK_PASTE, N_("Paste _Keys"), "<control>V",
				N_("Import keys from the clipboard"), null }
		};
		
		private static const Gtk.ActionEntry[] SERVER_ENTRIES = {
			{ "remote-find", Gtk.STOCK_FIND, N_("_Find Remote Keys..."), "",
				N_("Search for keys on a key server"), null }, 
			{ "remote-sync", Gtk.STOCK_REFRESH, N_("_Sync and Publish Keys..."), "",
				N_("Publish and/or synchronize your keys with those online."), null }
		}; 
		
		private static const Gtk.ToggleActionEntry[] VIEW_ENTRIES = {
			{ "view-type", null, N_("T_ypes"), null, N_("Show type column"), null, false },
			{ "view-expires", null, N_("_Expiry"), null, N_("Show expiry column"), null, false },
			{ "view-trust", null, N_("_Trust"), null, N_("Show owner trust column"), null, false },
			{ "view-validity", null, N_("_Validity"), null, N_("Show validity column"), null, false }
		};
		
		KeyManager(string ident = "key-manager") {
			this.name = ident;
		}
		
		construct {
			_loaded_gnome_keyring = false;
			_tabs = new TabInfo[(int)Tabs.NUM_TABS];
			
			_notebook = (Gtk.Notebook)get_widget ("notebook");
			
			window.set_title(_("Passwords and Encryption Keys"));

			/* 
			 * We hook callbacks up here for now because of a compiler warning. See:
			 * http://bugzilla.gnome.org/show_bug.cgi?id=539483
			 */
			 
			var actions = new Gtk.ActionGroup ("general");
			actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			actions.add_actions (GENERAL_ENTRIES, this);
			actions.get_action ("app-quit").activate += on_app_quit;
			actions.get_action ("key-generate").activate += on_key_generate;
			actions.get_action ("key-import-file").activate += on_key_import_file;
			actions.get_action ("key-import-clipboard").activate += on_key_import_clipboard;
			include_actions (actions);

			actions = new Gtk.ActionGroup ("keyserver");
			actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			actions.add_actions (SERVER_ENTRIES, this);
			actions.get_action ("remote-find").activate += on_remote_find;
			actions.get_action ("remote-sync").activate += on_remote_sync;
			include_actions (actions);
			
			_view_actions = new Gtk.ActionGroup("view");
			_view_actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			_view_actions.add_toggle_actions (VIEW_ENTRIES, this);
			 
			Gtk.ToggleAction action = (Gtk.ToggleAction)_view_actions.get_action ("view-type");
			action.set_active (Seahorse.Conf.get_boolean (Seahorse.Conf.SHOW_TYPE_KEY));
			action.activate += on_view_type_activate;

			action = (Gtk.ToggleAction)_view_actions.get_action ("view-expires");
			action.set_active (Seahorse.Conf.get_boolean (Seahorse.Conf.SHOW_EXPIRES_KEY));
			action.activate += on_view_expires_activate;

			action = (Gtk.ToggleAction)_view_actions.get_action ("view-trust");
			action.set_active (Seahorse.Conf.get_boolean (Seahorse.Conf.SHOW_TRUST_KEY));
			action.activate += on_view_trust_activate;

			action = (Gtk.ToggleAction)_view_actions.get_action ("view-validity");
			action.set_active (Seahorse.Conf.get_boolean (Seahorse.Conf.SHOW_VALIDITY_KEY));
			action.activate += on_view_validity_activate;

			include_actions (_view_actions);

			/* Notify us when gconf stuff changes under this key */
			Seahorse.Conf.notify_lazy (Seahorse.Conf.LISTING_SCHEMAS, 
			                            on_gconf_notify, this);
			                            
			
			/* close event */
			get_toplevel().delete_event += on_delete_event;
			
			/* first time signals */
			((Gtk.Button)get_widget("import-button")).clicked += on_import_button_clicked;
			((Gtk.Button)get_widget("new-button")).clicked += on_new_button_clicked;
			
			/* The notebook */
			_notebook.switch_page += on_tab_changed;
			
			/* Flush all updates */
			ensure_updated ();
			
			/* Find the toolbar */
			Gtk.Widget? widget = get_widget ("toolbar-placeholder");
			if (widget != null) {
				weak GLib.List<Widget> children = ((Gtk.Container)widget).get_children ();
				if (children != null && children.data != null) {
			
					/* The toolbar is the first (and only) element */
					Gtk.Toolbar? toolbar = (Gtk.Toolbar)children.data;
					if (toolbar != null && toolbar.get_type() == typeof (Gtk.Toolbar)) {
			
						/* Insert a separator to right align the filter */
						Gtk.SeparatorToolItem sep = new Gtk.SeparatorToolItem ();
						sep.set_draw (false);
						sep.set_expand (true);
						sep.show_all ();
						toolbar.insert (sep, -1);
			
						/* Insert a filter bar */
						Gtk.Box box = new Gtk.HBox (false, 0);
						box.pack_start (new Gtk.Label (_("Filter:")), false, true, 3);
						_filter_entry = new Gtk.Entry();
						box.pack_start (_filter_entry, false, true, 0);
						box.pack_start (new Gtk.Label (null), false, false, 0);
						box.show_all ();
			
						Gtk.ToolItem item = new Gtk.ToolItem ();
						item.add (box);
						item.show_all ();
						toolbar.insert (item, -1);
					}
				}
			}
			
			/* For the filtering */
			_filter_entry.changed += on_filter_changed;

			/* Initialize the tabs, and associate them up */
			initialize_tab ("pub-key-tab", Tabs.PUBLIC, "pub-key-list", PRED_PUBLIC);
			initialize_tab ("trust-key-tab", Tabs.TRUSTED, "trust-key-list", PRED_TRUSTED);
			initialize_tab ("sec-key-tab", Tabs.PRIVATE, "sec-key-list", PRED_PRIVATE);
			initialize_tab ("password-tab", Tabs.PASSWORD, "password-list", PRED_PASSWORD);
						
			/* Set focus to the current key list */
			widget = get_current_view ();
			widget.grab_focus ();

			/* To avoid flicker */
			base.show ();
			
			/* Setup drops */
			Gtk.TargetEntry[] entries = { };
			Gtk.drag_dest_set (window, Gtk.DestDefaults.ALL, entries, Gdk.DragAction.COPY);
			Gtk.TargetList targets = new Gtk.TargetList (entries, 0);
			targets.add_uri_targets (Targets.URIS);
			targets.add_text_targets (Targets.PLAIN);
			Gtk.drag_dest_set_target_list (window, targets);
			
			window.drag_data_received += on_target_drag_data_received;
			
			/* To show first time dialog */
			GLib.Timeout.add (1000, on_first_timer);
			
			selection_changed ();
		}
		
		public static weak Gtk.Window show (Operation op) {
			KeyManager man = new KeyManager ();
			man.ref(); /* Destorys itself with destroy */
			Progress.status_set_operation (man, op);
			return (Gtk.Window)man.get_toplevel();
		}
		
		public override List<weak Object> get_selected_objects () {
			TabInfo* tab = get_tab_info ();
			if (tab == null)
				return new List<weak Object>();
			return KeyManagerStore.get_selected_keys (tab->view);
		}
		
		public override void set_selected_objects (List<Object> objects) {
			List<weak Object>[] tab_lists = new List<weak Object>[(int)Tabs.NUM_TABS];
			
			/* Break objects into what's on each tab */
			foreach (Object obj in objects) {
				TabInfo* tab = get_tab_for_object (obj);
				if (tab == null)
					continue;
				
				assert (tab->id < (int)Tabs.NUM_TABS);
				tab_lists[tab->id].prepend (obj);
			}
			
			uint highest_matched = 0;
			TabInfo* highest_tab = null;
			for (int i = 0; i < (int)Tabs.NUM_TABS; ++i) {
				weak List<weak Object> list = tab_lists[i];
				TabInfo* tab = &_tabs[i];
				
				/* Save away the tab that had the most objects */
				uint num = list.length();
				if (num > highest_matched) {
					highest_matched = num;
					highest_tab = tab;
				}
				
				/* Select the objects on that tab */
				KeyManagerStore.set_selected_keys (tab->view, list);
			}
			
			/* Change to the tab with the most objects */
			if (highest_tab != null)
				set_tab_current (highest_tab);
		}
		
		public override weak Object? selected {
			get {
				TabInfo* tab = get_tab_info ();
				if (tab == null)
					return null;
				return KeyManagerStore.get_selected_key (tab->view, null);
			}
			set {
				List<weak Object> objects = new List<weak Object>();
				objects.prepend (value);
				set_selected_objects (objects);
			}
		}
		
		public override weak Object? get_selected_object_and_uid (out uint uid) {
			TabInfo* tab = get_tab_info ();
			if (tab == null)
				return null;
			return KeyManagerStore.get_selected_key (tab->view, out uid);
		}
		
		private TabInfo* get_tab_for_object (Object obj) {
			for (int i = 0; i < (int)Tabs.NUM_TABS; ++i) {
				TabInfo* tab = &_tabs[i];
				if (tab->objects.has_object (obj))
					return tab;
			}
			return null;
		}
		
		private weak Gtk.TreeView? get_current_view () {
			TabInfo *tab = get_tab_info ();
			if (tab == null)
				return null;
			return tab->view;
		}
		
		private uint get_tab_id (TabInfo *tab) {
			if (tab == null)
				return 0;
			return tab->id;
		}
		
		private TabInfo* get_tab_info (int page = -1) {
			if (page < 0)
				page = _notebook.get_current_page ();
			if (page < 0)
				return null;
			
			for (int i = 0; i < (int)Tabs.NUM_TABS; ++i) {
				TabInfo* tab = &_tabs[i];
				if (tab->page == page)
					return tab;
			}
			
			return null;
		}
		
		private void set_tab_current (TabInfo* tab) {
			_notebook.set_current_page (tab->page);
			selection_changed ();
		}
		
		private void initialize_tab (string tabwidget, uint tabid, 
		                             string viewwidget, Object.Predicate pred) {
			
			assert (tabid < (int)Tabs.NUM_TABS);
			
			_tabs[tabid].id = tabid;
			_tabs[tabid].widget = get_widget (tabwidget);
			return_if_fail (_tabs[tabid].widget != null);
			
			_tabs[tabid].page = Bugs.notebook_page_num (_notebook, _tabs[tabid].widget);
			return_if_fail (_tabs[tabid].page >= 0);
			
			Set objects = new Set.full (ref pred);
			_tabs[tabid].objects = objects;
			
			/* init key list & selection settings */
			Gtk.TreeView view = (Gtk.TreeView)get_widget (viewwidget);
			_tabs[tabid].view = view;
			return_if_fail (view != null);
			view.get_selection().set_mode (Gtk.SelectionMode.MULTIPLE);
			view.get_selection().changed += on_view_selection_changed;
			view.row_activated += on_row_activated;
			view.button_press_event += on_key_list_button_pressed;
			view.popup_menu += on_key_list_popup_menu;
			view.realize();

			/* Add new key store and associate it */
			_tabs[tabid].store = new KeyManagerStore(objects, view);
		}

		private bool on_first_timer () {
			
			/* Although not all the keys have completed we'll know whether we have 
			 * any or not at this point */
			if (Context.for_app ().count == 0) {
				Gtk.Widget widget = get_widget ("first-time-box");
				widget.show ();
			}
			
			return false;
		}

		private void on_filter_changed (Gtk.Entry entry)
		{
			string text = entry.get_text();
			for(int i = 0; i < _tabs.length; ++i)
				_tabs[i].store.set("filter", text, null);
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
			Object obj = this.selected;
			if (obj != null) {
				show_context_menu (0, Gtk.get_current_event_time ());
				return true;
			}
			return false;
		} 

		private void on_key_generate (Action action) {
			GenerateSelect.show (window);
		}
		
		private void on_new_button_clicked (Gtk.Widget widget) {
			GenerateSelect.show (window);
		}
		
		private void imported_keys (Operation op)
		{
			if (!op.is_successful ()) {
				op.display_error (_("Couldn't import keys"), window);
				return;
			}

			set_status (_("Imported keys"));
		}
		
		private void import_files (string[] uris) {
			
			var mop = new MultiOperation ();
			var errmsg = new StringBuilder ();
			
			foreach (string uri in uris) {
				
				if (uri.len() == 0)
					continue;
					
				/* Figure out where to import to */
				Quark ktype = Util.detect_file_type (uri);
				if (ktype == 0) {
					errmsg.append_printf ("%s: Invalid file format\n", uri);
					continue;
				}
				
				/* All our supported key types have a local source */
				Source sksrc = Context.for_app().find_source (ktype, Location.LOCAL);
				return_if_fail (sksrc != null);
				
				try {
					var file = File.new_for_uri (uri);
					var input = file.read (null);
					
					Operation op = sksrc.import (input);
					mop.take (op);
					
				} catch (GLib.Error ex) {
					errmsg.append_printf ("%s: %s", uri, ex.message);
					continue;
				}
			}
			
			if (mop.is_running ()) {
				Progress.show (mop, _("Importing keys"), true);
				mop.watch (imported_keys, null);
			}
			
			if (errmsg.len == 0)
				Util.show_error (window, _("Couldn't import keys"), errmsg.str);
		}
		
		private void import_prompt () {
			Gtk.Dialog dialog = Util.chooser_open_new (_("Import Key"), window);
			Util.chooser_show_key_files (dialog);
			
			string uri = Util.chooser_open_prompt (dialog);
			if (uri != null) {
				string[] uris = { uri };
				import_files (uris);
			}
		}		
		
		private void on_key_import_file (Action action) {
			import_prompt ();	
		}
		
		private void on_import_button_clicked (Gtk.Widget widget) {
			import_prompt ();
		}
		
		private void import_text (string text) {
			
			long len = text.len ();
			Quark ktype = Util.detect_data_type (text, len);
			if (ktype == 0) {
				Util.show_error (window, _("Couldn't import keys"),
				                 _("Unrecognized key type, or invalid data format"));
				return;
			}
			
			/* All our supported key types have a local key source */
			Source sksrc = Context.for_app().find_source (ktype, Location.LOCAL);
			return_if_fail (sksrc != null);

			/* 
			 * BUG: We cast to get around this bug:
			 * http://bugzilla.gnome.org/show_bug.cgi?id=540662
			 */
			var input = (MemoryInputStream)new MemoryInputStream.from_data (text, len, g_free);
			Operation op = sksrc.import (input);
			
			Progress.show (op, _("Importing Keys"), true);
			op.watch (imported_keys, null);
		}
		
		private void on_target_drag_data_received (Gtk.Window window, Gdk.DragContext context, 
		                                           int x, int y, Gtk.SelectionData selection_data, 
		                                           uint info, uint time_) {
			if (info == Targets.PLAIN) {
				import_text ((string)Bugs.selection_data_get_text (selection_data));
			} else if (info == Targets.URIS) {
				string[] uris = Bugs.selection_data_get_uris (selection_data);
				for (int i = 0; i < uris.length; ++i)
					uris[i] = uris[i].strip();
				import_files (uris);
			}
		}

		private void clipboard_received (Gtk.Clipboard board, string text) {
			if (text != null && text.len () > 0)
				import_text (text);
		}
		
		private void on_key_import_clipboard (Action action) {
			
			Gdk.Atom atom = Gdk.Atom.intern ("CLIPBOARD", false);
			Gtk.Clipboard board = Gtk.Clipboard.get (atom);
			board.request_text (clipboard_received);
		}

		private void on_remote_find (Action action) {
			KeyserverSearch.show (window);
		}
		
		private void on_remote_sync (Action action) {
			
			var objects = get_selected_objects ();
			if (objects == null)
				objects = Context.for_app().find_objects (0, 0, Location.LOCAL);
			KeyserverSync.show (objects, window);
		}
		
		private void on_app_quit (Action? action) {
			Context.for_app().destroy();
		}
		
		/* When this window closes we quit seahorse */	
		private bool on_delete_event (Gtk.Widget widget, Gdk.Event event) {
			on_app_quit (null);
			return true;
		}

		private void on_view_type_activate (ToggleAction action) {
			Seahorse.Conf.set_boolean(Seahorse.Conf.SHOW_TYPE_KEY, action.get_active());
		}		

		private void on_view_expires_activate (ToggleAction action) {
			Seahorse.Conf.set_boolean(Seahorse.Conf.SHOW_EXPIRES_KEY, action.get_active());
		}		

		private void on_view_validity_activate (ToggleAction action) {
			Seahorse.Conf.set_boolean(Seahorse.Conf.SHOW_VALIDITY_KEY, action.get_active());
		}		

		private void on_view_trust_activate (ToggleAction action) {
			Seahorse.Conf.set_boolean(Seahorse.Conf.SHOW_TRUST_KEY, action.get_active());
		}
		
		private void on_gconf_notify (GConf.Client client, uint cnxn_id, GConf.Entry entry) {
			
			Gtk.ToggleAction action = null;
			string key = entry.key;
			string name;
			
			if (key == Seahorse.Conf.SHOW_TRUST_KEY)
				name = "view-trust";
			else if (key == Seahorse.Conf.SHOW_TYPE_KEY)
				name = "view-type";
			else if (key == Seahorse.Conf.SHOW_EXPIRES_KEY)
				name = "view-expires";
			else if (key == Seahorse.Conf.SHOW_VALIDITY_KEY)
				name = "view-validity";
			else
				return;
				
			action = (Gtk.ToggleAction)_view_actions.get_action (name);
			return_if_fail (action != null);
			action.set_active (entry.value.get_bool ());
		}
		
		private bool fire_selection_changed () {

			bool selected = false;
			int rows = 0;
			
			Gtk.TreeView view = get_current_view ();
			if (view != null) {
				Gtk.TreeSelection selection = view.get_selection();
				rows = selection.count_selected_rows ();
				selected = rows > 0;
    			}
			
			bool dotracking = true;
    
    			/* See which tab we're on, if different from previous, no tracking */
    			uint tabid = get_tab_id (get_tab_info ());
    			if (tabid != _track_selected_tab) {
    				dotracking = false;
    				_track_selected_tab = tabid;
    			}

			/* Retrieve currently tracked, and reset tracking */
			Quark keyid = _track_selected_id;
			_track_selected_id = 0;
    
			/* no selection, see if selected key moved to another tab */
			if (dotracking && rows == 0 && keyid != 0) {

			        /* Find it */
				Object obj = Context.for_app().find_object (keyid, Location.LOCAL);
				if (obj != null) { 
				
					/* If it's still around, then select it */
					TabInfo* tab = get_tab_for_object (obj);
					if (tab != null && tab != get_tab_info ()) {

						/* Make sure we don't end up in a loop  */
						assert (_track_selected_id == 0);
						this.selected = obj;
					}
				}
			}
    
    			if (selected) {
        			set_numbered_status (Bugs.ngettext ("Selected %d key",
                                	             "Selected %d keys", rows), rows);
        
				List<weak Object> objects = get_selected_objects ();

				/* If one key is selected then mark it down for following across tabs */
				if (objects.data != null)
					_track_selected_id = objects.data.id;
					
			}
    
    			/* Fire the signal */
    			this.selection_changed ();
    			
			/* This is called as a one-time idle handler, return FALSE so we don't get run again */
		    	return false;
		}
		
		private void on_tab_changed (Gtk.Notebook notebook, void *unused, uint page_num) {

			_filter_entry.set_text ("");

			/* Don't track the selected key when tab is changed on purpose */
			_track_selected_id = 0;
			fire_selection_changed ();
    
			/* 
			 * Because gnome-keyring can throw prompts etc... we delay loading 
			 * of the gnome key ring items until we first access them. 
			 */
    
    			if (get_tab_id (get_tab_info ((int)page_num)) == Tabs.PASSWORD)
        			load_gnome_keyring_items ();
		}

		private void load_gnome_keyring_items () {
			
			if (_loaded_gnome_keyring)
				return;
			
			GLib.Type type = Registry.get().find_type ("gnome-keyring", "local", "key-source", null);
			return_if_fail (type != 0);

			var sksrc = (Source)GLib.Object.new (type, null);
			Context.for_app().add_source (sksrc);
			Operation op = sksrc.load (0);
			
			/* Monitor loading progress */
			Progress.status_set_operation (this, op);
		}
	}
}
