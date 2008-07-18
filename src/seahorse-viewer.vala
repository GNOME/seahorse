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
	public abstract class Viewer : Widget, View {

		private UIManager _ui_manager;
		private ActionGroup _object_actions;
		private HashTable<Quark, Commands> _commands;
		private static bool _about_initialized;
		
		private static const ActionEntry[] UI_ENTRIES = {

			/* Top menu items */
			{ "key-menu", null, N_("_Key") },
			{ "edit-menu", null, N_("_Edit") },
			{ "view-menu", null, N_("_View") },
			{ "help-menu", null, N_("_Help") },

			{ "app-preferences", Gtk.STOCK_PREFERENCES, N_("Prefere_nces"), null,
			  	N_("Change preferences for this program"), null },
			{ "app-about", "gnome-stock-about", N_("_About"), null, 
			  	N_("About this program"), null }, 
    			{ "help-show", Gtk.STOCK_HELP, N_("_Contents"), "F1",
			  	N_("Show Seahorse help"), null } 
		};
		
		private static const ActionEntry[] KEY_ENTRIES = {
			{ "key-properties", Gtk.STOCK_PROPERTIES, N_("P_roperties"), null,
			  	N_("Show key properties"), null }, 
			{ "key-export-file", Gtk.STOCK_SAVE_AS, N_("E_xport Public Key..."), null,
			  	N_("Export public part of key to a file"), null },
			{ "key-export-clipboard", Gtk.STOCK_COPY, N_("_Copy Public Key"), "<control>C",
            			N_("Copy public part of selected keys to the clipboard"), null },
			{ "key-delete", Gtk.STOCK_DELETE, N_("_Delete Key"), null,
            		  	N_("Delete selected keys"), null }
		};
		
		construct {
			_ui_manager = new Gtk.UIManager ();

			/* The widgts get added in an idle loop later */
			_ui_manager.add_widget += on_ui_add_widget;
			                             
			try {
				string path = "%sseahorse-%s.ui".printf (Config.SEAHORSE_GLADEDIR, name);
				_ui_manager.add_ui_from_file (path);
			} catch (Error ex) {
				warning ("couldn't load ui description for '%s': %s", name, ex.message);
			}
			
			var win = get_toplevel ();
			if (win.get_type() == typeof (Gtk.Window))
				((Gtk.Window)win).add_accel_group (_ui_manager.get_accel_group ());
				
			include_basic_actions ();
			this.selection_changed += on_selection_changed;
			
			/* Setup the commands */
			_commands = new HashTable<Quark, Commands> (GLib.direct_hash, GLib.direct_equal);
			var types = Registry.get().find_types ("commands", null);
			foreach (GLib.Type typ in types) {
			
				/* Add each commands to our hash table */
				Commands commands = (Commands)GLib.Object.new(typ, "view", this, null);
				_commands.insert (commands.ktype, commands);

				/* Add the UI for each commands */
				var actions = commands.command_actions;
				if (actions != null)
					include_actions (actions);
				var uidef = commands.ui_definition;
				if (uidef != null && uidef.len() > 0)
					 _ui_manager.add_ui_from_string (uidef, -1);

			}
		}
		
		private void include_basic_actions () {

			/*
			 * We hook callbacks up here for now because of a compiler warning. See:
			 * http://bugzilla.gnome.org/show_bug.cgi?id=539483
			 */

			var actions = new Gtk.ActionGroup ("main");
			actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			actions.add_actions (UI_ENTRIES, this);
			actions.get_action("app-preferences").activate += on_app_preferences;
			actions.get_action("app-about").activate += on_app_about;
			actions.get_action("help-show").activate += on_help_show;
			include_actions (actions);
		
			_object_actions = new Gtk.ActionGroup ("key");
			_object_actions.set_translation_domain (Config.GETTEXT_PACKAGE);
			_object_actions.add_actions (KEY_ENTRIES, this);
			_object_actions.get_action("key-properties").activate += on_key_properties;
			_object_actions.get_action("key-export-file").activate += on_key_export_file;
			_object_actions.get_action("key-export-clipboard").activate += on_key_export_clipboard;
			_object_actions.get_action("key-delete").activate += on_key_delete;

    			/* Mark the properties toolbar button as important */
    			_object_actions.get_action("key-properties").is_important = true;

			include_actions (_object_actions);
		} 
		
		protected void ensure_updated () {
			_ui_manager.ensure_update ();
		}
		
		protected void include_actions (Gtk.ActionGroup actions) {
			_ui_manager.insert_action_group (actions, -1);
		}

		public virtual List<weak Object> get_selected_objects () {
			return new List<weak Object>();
		}
		
		public virtual void set_selected_objects (List<Object> objects) {
			/* Must be overridden */
		}
		
		public virtual weak Object? selected {
			get {
				/* Must be overridden */
				return null;
			}
			set {
				List<weak Object> objects = new List<weak Object>();
				objects.prepend(value);
				set_selected_objects (objects);
			}
		}
		
		public virtual weak Set? current_set {
			get {
				/* Must be overridden */
				return null;
			}
		}
		
		public virtual weak Object? get_selected_object_and_uid (out uint uid) {
			/* Must be overridden */
			return null;
		}
		
		public Gtk.Window window { 
			get { return (Gtk.Window)get_toplevel(); }
		}
		
		private void on_ui_add_widget (Gtk.UIManager ui, Gtk.Widget widget) {
			
			string name = null;
			if (widget.get_type () == typeof (Gtk.MenuBar))
				name = "menu-placeholder";
			else if (widget.get_type () == typeof (Gtk.Toolbar))
				name = "toolbar-placeholder";
			else 
				return;
				
			Gtk.Widget holder = get_widget (name);
			if (holder != null)
				((Gtk.Container)holder).add (widget);
			else
				warning ("no place holder found for: %s", name);
		} 
		
		private void on_app_preferences (Action action) {
			Preferences.show (window, null);
		}
		
		private void on_app_about (Action action) {
		
			string[] authors = {
				"Jacob Perkins <jap1@users.sourceforge.net>",
				"Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
				"Jean Schurger <yshark@schurger.org>",
				"Stef Walter <stef@memberwebs.com>",
				"Adam Schreiber <sadam@clemson.edu>",
				"",
				_("Contributions:"),
				"Albrecht Dreß <albrecht.dress@arcor.de>",
				"Jim Pharis <binbrain@gmail.com>"
			};
			
			string[] documenters = {
				"Jacob Perkins <jap1@users.sourceforge.net>",
				"Adam Schreiber <sadam@clemson.edu>",
				"Milo Casagrande <milo_casagrande@yahoo.it>"
			};
			
			string[] artists = {
				"Jacob Perkins <jap1@users.sourceforge.net>",
				"Stef Walter <stef@memberwebs.com>"
			};
		
			if (!_about_initialized) {
				_about_initialized = true;
				Gtk.AboutDialog.set_url_hook (on_about_link_clicked, null);
			}
			
			Gtk.AboutDialog about = new Gtk.AboutDialog();
			about.artists = artists;
			about.authors = authors;
			about.documenters = documenters;
			about.version = Config.VERSION; 
			about.comments = _("Encryption Key Manager");
			about.copyright = "Copyright \xc2\xa9 2002 - 2008 Seahorse Project";
			about.translator_credits = _("translator-credits");
			about.logo_icon_name = "seahorse";
			about.website = "http://www.gnome.org/projects/seahorse";
			about.website_label = _("Seahorse Project Homepage");
			
			about.run();
		}
		
		private static void on_about_link_clicked (Gtk.AboutDialog about, string url) {
			try {
				GLib.AppInfo.launch_default_for_uri (url, new GLib.AppLaunchContext ());
			} catch (GLib.Error ex){
				warning ("couldn't launch url: %s: %s", url, ex.message);
			}
		}
		
		private void on_help_show (Action action) {
			show_help ();
		}
		
		protected void show_context_menu (uint button, uint time) {
			Gtk.Menu menu = (Gtk.Menu)_ui_manager.get_widget ("/KeyPopup");
			return_if_fail (menu != null && menu.get_type() == typeof (Gtk.Menu));
			
			menu.popup (null, null, null, button, time);
			menu.show ();
		}
		
		protected void show_properties (Object obj) {
			var commands = _commands.lookup(obj.tag);
			if (commands != null)
				commands.show_properties (obj);
		}
		
		private void on_key_properties (Action action) {
			if (this.selected != null)
				show_properties (this.selected);
		}
		
		private int compare_by_tag (Object one, Object two) {
			Quark kone = one.tag;
			Quark ktwo = two.tag;
			if (kone < ktwo)
				return -1;
			if (kone > ktwo)
				return 1;
			return 0;
		}
		
		private void delete_object_batch (List<Object> objects) {
			assert (objects != null);
			var commands = _commands.lookup(objects.data.tag);
			
			try {
				if (commands != null)
					commands.delete_objects (objects);
			} catch (GLib.Error ex) {
				Util.handle_error (ex, _("Couldn't delete."), window);
			}
		}
		
		private void on_key_delete (Action action) {
			List<weak Object> objects, batch;
			
			/* Get the selected objects and sort them by ktype */
			objects = get_selected_objects ();
			objects.sort ((GLib.CompareFunc)compare_by_tag);

			uint num = objects.length();
			if (num == 0)
				return;
			
			/* Check for private objects */
			foreach (var object in objects) {
				if (object.usage == Usage.PRIVATE_KEY) {
					string prompt;
					if (num == 1)
						prompt = _("%s is a private key. Are you sure you want to proceed?").printf(objects.data.display_name);
					else
						prompt = _("One or more of the deleted keys are private keys. Are you sure you want to proceed?");
					if (!Util.prompt_delete (prompt))
						return;
				}
			}
			
			Quark ktype = 0;
			batch = new List<weak Object>();
			foreach (Object object in objects) {
			
				/* Process that batch */
				if (ktype != object.tag && batch != null) {
				 	delete_object_batch (batch);
					batch = new List<weak Object>();
				}

				/* Add to the batch */				
				batch.prepend (object);
			}
			
			/* Process last batch */
			if (batch != null)
				delete_object_batch (batch);
		}
		
		private void on_copy_complete (Operation op) {
		
			if (!op.is_successful ()) {
				op.display_error (_("Couldn't retrieve data from key server"), window);
				return;
			}
			
			GLib.Object result = (GLib.Object)op.get_result ();
			return_if_fail (result != null && result.get_type () != typeof (MemoryOutputStream));
			MemoryOutputStream output = (MemoryOutputStream)result;
			
			weak string text = (string)output.get_data ();
			return_if_fail (text != null);
			
			uint size = Util.memory_output_length (output);
			return_if_fail (size >= 0);
			
			Gdk.Atom atom = Gdk.Atom.intern ("CLIPBOARD", false);
			Gtk.Clipboard board = Gtk.Clipboard.get (atom);
			board.set_text (text, (int)size);
			
        		set_status (_("Copied keys"));
		}
		
		private void on_key_export_clipboard (Action action) {
		
			var objects = get_selected_objects ();
			if (objects == null)
				return;
				
			OutputStream output = new MemoryOutputStream (null, 0, realloc, free);
			Operation op = Source.export_objects (objects, output);
			
			Progress.show (op, _("Retrieving keys"), true);
			op.watch (on_copy_complete, null);
		}
		
		private void on_export_done (Operation op) {
			if (!op.is_successful ())
				op.display_error (_("Couldn't export keys"), window);
		}

		private void on_key_export_file (Action action) {
		
			var objects = get_selected_objects ();
			if (objects == null)
				return;
			
			Gtk.Dialog dialog = Util.chooser_save_new (_("Export public key"), window);
			Util.chooser_show_key_files (dialog);
			Util.chooser_set_filename (dialog, objects);
			
			string uri = Util.chooser_save_prompt (dialog);
			if (uri != null) {
			
				try {
					var file = File.new_for_uri (uri);
					OutputStream output = file.replace (null, false, 0, null);
					
					Operation op = Source.export_objects (objects, output);
					Progress.show (op, _("Exporting keys"), true);
					op.watch (on_export_done, null);
					
				} catch (Error ex) {
					Util.handle_error (ex, _("Couldn't export key to \"%s\""),
					                   Util.uri_get_last (uri));
					return;
				}
				
			}
		}
		
		private void on_selection_changed (View view) {
			_object_actions.set_sensitive (view.selected != null);
		}
		
		protected void set_status (string text) {
			Gtk.Widget widget = get_widget ("status");
			return_if_fail (widget != null || widget.get_type() != typeof (Gtk.Statusbar));
			
			Gtk.Statusbar status = (Gtk.Statusbar)widget;
			uint id = status.get_context_id ("key-manager");
			status.pop (id);
			status.push (id, text);
		}
		
		protected void set_numbered_status (string text, int num) {
			string message = text.printf (num);
			set_status (message);
		}
	}
}
