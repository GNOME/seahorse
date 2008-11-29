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
 
[CCode (cprefix = "SeahorseSSH", lower_case_cprefix = "seahorse_ssh_")]
namespace Seahorse.Ssh {
        [CCode (cheader_filename = "seahorse-ssh-dialogs.h")]
        public class Upload : GLib.Object {
		public static void prompt (GLib.List<Key> keys, Gtk.Window? window);
        }

        [CCode (cheader_filename = "seahorse-ssh-dialogs.h")]
        public class Generate : GLib.Object {
		public static void show (Ssh.Source sksrc, Gtk.Window? window);
        }

        [CCode (cheader_filename = "seahorse-ssh-dialogs.h")]
        public class KeyProperties : GLib.Object {
		public static void show (Ssh.Key key, Gtk.Window? window);
        }
                
        [CCode (cheader_filename = "seahorse-ssh-source.h")]
        public class Source : Seahorse.Source {
        	
        }
        
        [CCode (cheader_filename = "seahorse-ssh-key.h")]
        public class Key : Seahorse.Object {
        
        }
}

