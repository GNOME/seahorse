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

using GLib;
using Gtk;

namespace Seahorse.Pgp {
	public class Commands : Seahorse.Commands {
	
		private static const ActionEntry[] COMMAND_ENTRIES = {
			{ "key-sign", Gtk.STOCK_INDEX,  N_("_Sign Key..."), "",
			  N_("Sign public key"), null }
		};

		private ActionGroup _command_actions = null;
		
		static construct {
			/* Register this class as a commands */
			Registry.get().register_type(typeof(Commands), Pgp.TYPE_STR, "commands", null);
		}

		construct {
			assert (view != null);	
			view.selection_changed += on_view_selection_changed; 	
		}
		
		public override GLib.Quark ktype { 
			get { return Seahorse.Pgp.TYPE; } 
		} 
		
		public override string# ui_definition { 
			get { return UI_DEF; } 
		}
		
		public override ActionGroup command_actions {
			get {
				if (_command_actions == null) {
					_command_actions = new ActionGroup ("pgp");
					_command_actions.set_translation_domain (Config.GETTEXT_PACKAGE);
					_command_actions.add_actions (COMMAND_ENTRIES, this);
					_command_actions.get_action("key-sign").activate += on_key_sign;
				}
				return _command_actions;
			}
		}
		
		public override void show_properties (Object obj) {
			return_if_fail (obj.tag == Seahorse.Pgp.TYPE);
			if (obj.get_type() == typeof (Pgp.Uid))
				obj = obj.parent;
			return_if_fail (obj.get_type() == typeof (Pgp.Key));
			KeyProperties.show ((Pgp.Key)obj, view.window);
		}
		
		public override void delete_objects (List<Object> objects) throws GLib.Error {
			uint num = objects.length();
			if (num == 0)
				return;
			
			foreach (var obj in objects) {

				/* 
				 * Delete all the user ids first, if parent key is 
				 * not on the chopping block already.
				 */

				if (obj.get_type() == typeof (Pgp.Uid)) {
					Pgp.Uid uid = (Pgp.Uid)obj;
					if (objects.find(uid.parent) == null)
						Delete.userid_show ((Pgp.Key)uid.parent, uid.index);
					objects.remove (obj);
				} else {
					return_if_fail (obj.get_type() != typeof (Pgp.Key));
				}
			}
			
			Delete.show(objects);
		}

		private void on_key_sign (Action action) {
			var key = view.selected;
			/* TODO: Make signing a specific UID work again */
			if (key != null && key.tag == Seahorse.Pgp.TYPE)
				Sign.prompt ((Pgp.Key)key, 0, view.window);
		}
		
		private void on_view_selection_changed (View view) {
			var keys = view.get_selected_objects ();
			bool enable = (keys != null);
			foreach (var key in keys) {
				if (key.tag != Seahorse.Pgp.TYPE) {
					enable = false;
					break;
				}
			}
			
			_command_actions.set_sensitive(enable);
		}
		
		/* 
		 * We put this at the end, because otherwise multiline strings mess up 
		 * the line number that the compiler reports errors on. See bug:
		 * http://bugzilla.gnome.org/show_bug.cgi?id=539491
		 */
		private static const string UI_DEF = """
			<ui>
			
			<menubar>
				<menu name='Key' action='key-menu'>
					<placeholder name="KeyCommands">
						<menuitem action="key-sign"/>
					</placeholder>
				</menu>
			</menubar>
			
			<toolbar name="MainToolbar">
				<placeholder name="ToolItems">
					<toolitem action="key-sign"/>
				</placeholder>
			</toolbar>

			<popup name="KeyPopup">
				<menuitem action="key-sign"/>
			</popup>
    
			</ui>
		""";
	}
}
