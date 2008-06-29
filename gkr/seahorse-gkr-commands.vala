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
 
namespace Seahorse.Gkr {
	public class Commands : Seahorse.Commands {
	
		private ActionGroup _actions;
		
		static construct {
			/* Register this class as a commands */
			Registry.get().register_type(typeof(Commands), Gkr.TYPE_STR, "commands", null);
		}
		
		public override GLib.Quark ktype { 
			get { return Seahorse.Gkr.TYPE; } 
		} 
		
		public override string# ui_definition { 
			get { return ""; } 
		}
		
		public override ActionGroup command_actions {
			get { 
				if (_actions == null)
					_actions = new ActionGroup("gkr");
				return _actions;
			}
		}
		
		public override void show_properties (Seahorse.Key key) {
			return_if_fail (key.ktype == Seahorse.Gkr.TYPE);
			ItemProperties.show ((Gkr.Item)key, view.window);
		}
		
		public override void delete_keys (List<Key> keys) throws GLib.Error {
			uint num = keys.length();
			if (num == 0)
				return;
			
			string prompt;
			if (num == 1)
				prompt = _("Are you sure you want to delete the password '%s'?").printf(keys.data.display_name);
			else
				prompt = _("Are you sure you want to delete %d passwords?").printf(num);
			
			if (Util.prompt_delete (prompt))
				KeySource.delete_keys (keys);
		}
	}
}
