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
 
[CCode (cprefix = "Seahorse", lower_case_cprefix = "seahorse_")]
namespace Seahorse {

        [CCode (cheader_filename = "seahorse-source.h")]
        public class Source : GLib.Object {
		public GLib.Quark ktype { get; }
		public static Operation export_objects (GLib.List<Object> objects, GLib.OutputStream output);
		public static void delete_objects (GLib.List<Object> objects) throws GLib.Error;
		
		public virtual Operation load (GLib.Quark id);
		public virtual Operation search (string match);
		public virtual Operation import (GLib.InputStream input);
		public virtual Operation export (GLib.List<weak Object> objects, bool complete, GLib.OutputStream output);
		public virtual Operation export_raw (GLib.List<GLib.Quark> ids, GLib.OutputStream output);
		public virtual void remove (Object object, uint name) throws GLib.Error; 
        }
        
        [CCode (cheader_filename = "seahorse-key.h")]
        public class Key : Seahorse.Object {

	       	[CCode (cprefix = "SKEY_FLAG_")]
        	public static enum Flag {
			IS_VALID =    0x0001,
			CAN_ENCRYPT = 0x0002,
			CAN_SIGN =    0x0004,
			EXPIRED =     0x0100,
			REVOKED =     0x0200,
			DISABLED =    0x0400,
			TRUSTED =     0x1000
        	}

		public GLib.Quark ktype { get; }
		public GLib.Quark keyid { get; }
		public Usage usage { get; }
		public string display_name { get; }
        }
        
	[CCode (cheader_filename = "seahorse-set.h")]
	public class Set : GLib.Object {
		public Set.full (ref Object.Predicate pred); 
		public bool has_object (Object obj);
	}
	
	public delegate void DoneFunc (Operation op);
	public delegate void ProgressFunc (Operation op, string text, double value);

	[CCode (cheader_filename = "seahorse-operation.h")]
	public class Operation : GLib.Object {
		public Operation.complete();
		public bool is_successful ();
		public bool is_running ();
		public void display_error (string heading, Gtk.Widget? parent);
		public void* get_result ();
		public void watch (DoneFunc? done, ProgressFunc? progress);
	}

	[CCode (cheader_filename = "seahorse-operation.h")]
	public class MultiOperation : Operation {
		public void take (Operation op);
	}
	
	[CCode (cheader_filename = "seahorse-context.h")]
	public class Context : Gtk.Object {
		public static weak Context for_app ();
		public uint count { get; }
		public weak Source find_source (GLib.Quark ktype, Location location);
		public void add_source (Source sksrc);
		public weak Object? find_object (GLib.Quark ktype, Location loc);
		public GLib.List<weak Object> find_objects (GLib.Quark ktype, Usage usage, Location loc);
		public Operation transfer_objects (GLib.List<Object> objects, Source? to);
		public void destroy ();		
	}

	[CCode (cheader_filename = "common/seahorse-registry.h")]
	public class Registry : GLib.Object {
		public static weak Registry get ();
		public GLib.List<GLib.Type> find_types (string category, ...);
		public GLib.Type find_type (string category, ...);
		public void register_type (GLib.Type type, string category, ...);
	}
	
	[CCode (cheader_filename = "seahorse-widget.h")]
	public class Widget : Gtk.Object {
		public string name { get; construct; }
		public weak Gtk.Widget get_toplevel ();
		public weak Gtk.Widget get_widget (string name);
		public void show_help ();
		public void show ();
		public void destroy ();
	}
	
	[CCode (cheader_filename = "seahorse-preferences.h")]
	namespace Preferences {
		public void show (Gtk.Window parent, string? focus);
	}

	[CCode (cheader_filename = "seahorse-progress.h")]
	namespace Progress {
		public void show (Operation op, string title, bool delayed);
		public void status_set_operation (Widget widget, Operation op);
	}
	
	[CCode (cheader_filename = "seahorse-util.h")]
	namespace Util {
		public uint memory_output_length (GLib.MemoryOutputStream output);
		
		public weak Gtk.Dialog chooser_save_new (string title, Gtk.Window? parent);
		public void chooser_show_key_files (Gtk.Dialog dialog);
		public void chooser_set_filename (Gtk.Dialog dialog, GLib.List<Key> keys);
		public string chooser_save_prompt (Gtk.Dialog dialog);
		
		public weak Gtk.Dialog chooser_open_new (string title, Gtk.Window? parent);
		public string chooser_open_prompt (Gtk.Dialog dialog);
		
		public void handle_error (GLib.Error ex, string format, ...);
		public void show_error (Gtk.Widget parent, string heading, string description);
		public bool prompt_delete (string text);
		
		public string uri_get_last (string uri);
		public GLib.Quark detect_file_type (string uri);
		public GLib.Quark detect_data_type (string text, long len);	
	}

	[CCode (cheader_filename = "seahorse-gconf.h", cprefix = "SeahorseGConf", lower_case_cprefix = "seahorse_gconf_")]
	namespace Conf {
		[CCode (cname = "SHOW_TRUST_KEY", cheader_filename = "seahorse-preferences.h")]
		public const string SHOW_TRUST_KEY;
		
		[CCode (cname = "SHOW_VALIDITY_KEY", cheader_filename = "seahorse-preferences.h")]
		public const string SHOW_VALIDITY_KEY;
		
		[CCode (cname = "SHOW_EXPIRES_KEY", cheader_filename = "seahorse-preferences.h")]
		public const string SHOW_EXPIRES_KEY;
		
		[CCode (cname = "SHOW_TYPE_KEY", cheader_filename = "seahorse-preferences.h")]
		public const string SHOW_TYPE_KEY;
		
		[CCode (cname = "LISTING_SCHEMAS", cheader_filename = "seahorse-preferences.h")]
		public const string LISTING_SCHEMAS;
		
		[CCode (cname = "KEYSERVER_KEY", cheader_filename = "seahorse-gconf.h")]
		public const string KEYSERVER_KEY;		
		
		public bool get_boolean (string key);
		public void set_boolean (string key, bool value);
		public GLib.SList<string> get_string_list (string key);
		public void notify_lazy (string key, GConf.ClientNotifyFunc func, Gtk.Object lifetime);
	}
	
	[CCode (cheader_filename = "seahorse-key-manager-store.h")]
	public class KeyManagerStore : GLib.Object {
		public KeyManagerStore(Set set, Gtk.TreeView view);
		public static GLib.List<weak Object> get_selected_objects (Gtk.TreeView view);
		public static void set_selected_objects (Gtk.TreeView view, GLib.List<Object> objects);
		public static weak Object? get_selected_object (Gtk.TreeView view);
		public static weak Object? get_object_from_path (Gtk.TreeView view, Gtk.TreePath path);
	}
	
	[CCode (cheader_filename = "seahorse-windows.h")]
	public class KeyserverSearch : GLib.Object {
		public static weak Gtk.Window show (Gtk.Window parent);
	}
	
	[CCode (cheader_filename = "seahorse-windows.h")]
	public class KeyserverSync : GLib.Object {
		public static weak Gtk.Window show (GLib.List<Key> keys, Gtk.Window parent);
	}
}
