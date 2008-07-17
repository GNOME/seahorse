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
	public class Generator : Seahorse.Generator {

		private static const ActionEntry[] GENERATE_ENTRIES = {
			{ "pgp-generate-key", Seahorse.Pgp.STOCK_ICON, N_("PGP Key"), "",
			  N_("Used to encrypt email and files"), null }
		};
		
		private ActionGroup _generate_actions = null;
		
		static construct {
			/* Register this class as a commands */
			Registry.get().register_type(typeof(Generator), Pgp.TYPE_STR, "generator", null);
		}

		public override ActionGroup actions {
			get {
				if (_generate_actions == null) {
					_generate_actions = new ActionGroup ("pgp-generate");
					_generate_actions.set_translation_domain (Config.GETTEXT_PACKAGE);
					_generate_actions.add_actions (GENERATE_ENTRIES, this);
					_generate_actions.get_action("pgp-generate-key").activate += on_pgp_generate;
				}
				return _generate_actions;
			}
		}

		private void on_pgp_generate (Action action) {
			var sksrc = Context.for_app().find_source(Seahorse.Pgp.TYPE, Location.LOCAL);
			GLib.return_if_fail (sksrc != null);
			Generate.show ((Pgp.Source)sksrc, null);
		}
	}
}
