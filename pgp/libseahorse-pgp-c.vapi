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
 
[CCode (cprefix = "SeahorsePGP", lower_case_cprefix = "seahorse_pgp_")]
namespace Seahorse.Pgp {
	
        [CCode (cheader_filename = "seahorse-pgp-dialogs.h")]
        public class Sign : GLib.Object {
		public static void prompt (Key key, uint uid, Gtk.Window parent);
        }

        [CCode (cheader_filename = "seahorse-pgp-dialogs.h")]
        public class Delete : GLib.Object {
		public static void show (GLib.List<Key> keys);
		public static void userid_show (Key key, uint index);
        }

        [CCode (cheader_filename = "seahorse-pgp-dialogs.h")]
        public class Generate : GLib.Object {
		public static void show (Pgp.Source sksrc, Gtk.Window? parent);
        }
        
        [CCode (cheader_filename = "seahorse-pgp-dialogs.h")]
        public class KeyProperties : GLib.Object {
		public static void show (Pgp.Key key, Gtk.Window? parent);
        }
                
        [CCode (cheader_filename = "seahorse-pgp-source.h")]
        public class Source : Seahorse.Source {
        	
        }
        
        [CCode (cheader_filename = "seahorse-pgp-key.h")]
        public class Key : Seahorse.Object {
        
        }

	[CCode (cheader_filename = "seahorse-pgp-uid.h")]
	public class Uid : Seahorse.Object {
		public uint index { get; }
	}
}

