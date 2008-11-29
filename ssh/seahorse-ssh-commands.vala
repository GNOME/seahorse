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

namespace Seahorse.Ssh {
	public class Commands : Seahorse.Commands {
	
		private static const ActionEntry[] COMMAND_ENTRIES = {
			{ "remote-ssh-upload", null, N_("Configure Key for _Secure Shell..."), "",
			  N_("Send public Secure Shell key to another machine, and enable logins using that key."), null }
		};

		private ActionGroup _command_actions = null;
		
		static construct {
			/* Register this class as a commands */
			Registry.get().register_type(typeof(Commands), Ssh.TYPE_STR, "commands", null);
		}
		
		construct {
			assert (view != null);	
			view.selection_changed += on_view_selection_changed; 	
		}
		
		public override GLib.Quark ktype { 
			get { return Seahorse.Ssh.TYPE; } 
		} 
		
		public override weak string ui_definition { 
			get { return UI_DEF; } 
		}
		
		public override ActionGroup command_actions {
			get {
				if (_command_actions == null) {
					_command_actions = new ActionGroup ("ssh");
					_command_actions.set_translation_domain (Config.GETTEXT_PACKAGE);
					_command_actions.add_actions (COMMAND_ENTRIES, this);
					_command_actions.get_action("remote-ssh-upload").activate += on_ssh_upload;
				}
				return _command_actions;
			}
		}

		public override void show_properties (Object key) {
			return_if_fail (key.tag == Seahorse.Ssh.TYPE);
			KeyProperties.show ((Ssh.Key)key, view.window);
		}

		public override Operation? delete_objects (List<Object> keys) {
			uint num = keys.length();
			if (num == 0)
				return null;
			
			string prompt;
			if (num == 1)
				prompt = _("Are you sure you want to delete the secure shell key '%s'?").printf(keys.data.label);
			else
				prompt = _("Are you sure you want to delete %d secure shell keys?").printf(num);
			
			if (!Util.prompt_delete (prompt))
				return null;
			
			return Seahorse.Source.delete_objects (keys);
		}
		
		private void on_ssh_upload (Action action) {
			List<weak Key> ssh_keys;
			var keys = view.get_selected_objects ();
			foreach (var key in keys) {
				if (key.tag == Seahorse.Ssh.TYPE && 
				    key.usage == Seahorse.Usage.PRIVATE_KEY)
					ssh_keys.append ((Ssh.Key)key);
			}
			
			Upload.prompt (keys, view.window);
		}
		
		private void on_view_selection_changed (View view) {
			var keys = view.get_selected_objects ();
			bool enable = (keys != null);
			foreach (var key in keys) {
				if (key.tag != Seahorse.Ssh.TYPE || 
				    key.usage != Seahorse.Usage.PRIVATE_KEY) {
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
				<menu name='Remote' action='remote-menu'>
					<menuitem action='remote-ssh-upload'/>
				</menu>
			</menubar>
			
			<popup name="KeyPopup">
				<menuitem action="remote-ssh-upload"/>
			</popup>
			
			</ui>
		""";
	}
}
