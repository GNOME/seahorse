/* 
 * These bindings will move into gnome-keyring once vala is more accepted in 
 * the desktop.
 */ 
[CCode (cheader_filename = "gp11.h", cprefix = "GP11", lower_case_cprefix = "gp11_")]
namespace GP11 {

	public weak string message_from_rv (uint rv);
	
	[CCode (dup_function = "gp11_attribute_dup", free_function = "gp11_attribute_free")]
	public class Attribute {
		public Attribute.new_boolean (uint attr_type, bool val);
		public Attribute.new_date (uint attr_type, GLib.Date date);
		public Attribute.new_ulong (uint attr_type, ulong val);
		public Attribute.new_string (uint attr_type, string val);
		public uint type { get; }
		public bool get_boolean();
		public ulong get_ulong();
		public string get_string();
		public void get_data(out void* data, out ulong length);
	}

	[CCode (ref_function = "gp11_attributes_ref", unref_function = "gp11_attributes_unref")]
	public class Attributes {
		public Attribute (void);
		public bool immutable { get; }
		public uint count { get; }
		public Attribute at(uint index);
		public void add(Attribute attr);
		public void add_data(uint attr_type, void* data, gulong length);
		public void add_boolean(uint attr_type, bool val);
		public void add_date (uint attr_type, GLib.Date date);
		public void add_ulong (uint attr_type, ulong val);
		public void add_string (uint attr_type, string val);
		public Attribute? find(uint attr_type);
		public bool find_boolean(uint attr_type, out bool val);
		public bool find_ulong(uint attr_type, out ulong val);
		public bool find_string(uint attr_type, out string val);
		public bool find_date(uint attr_type, out GLib.Date date);
	}	

	[CCode (dup_function = "gp11_module_info_dup", free_function = "gp11_module_info_free")]
	public class ModuleInfo {
		public uint pkcs11_version_major;
		public uint pkcs11_version_minor;
		public string manufacturer_id;
		public uint flags;
		public string library_description;
		public uint library_version_major;
		public uint library_version_minor;
	}

	public class Module : Glib.Object {
		public static Module initialize(string path) throws GLib.Error;

		public ModuleInfo info { get; }
		public GLib.List<Slot> get_slots();
	}

	[CCode (dup_function = "gp11_slot_info_dup", free_function = "gp11_slot_info_free")]
	public class SlotInfo {
		public string slot_description;
		public string manufacturer_id;
		public uint flags;
		public uint hardware_version_major;
		public uint hardware_version_minor;
		public uint firmware_version_major;
		public uint firmware_version_minor;
	}

	[CCode (dup_function = "gp11_token_info_dup", free_function = "gp11_token_info_free")]
	public class TokenInfo {
		public string label;
		public string manufacturer_id;
		public string model;
		public string serial_number;
		public uint flags;
		public ulong max_session_count;
		public ulong session_count;
		public ulong max_rw_session_count;
		public ulong max_pin_len;
		public ulong min_pin_len;
		public ulong total_public_memory;
		public ulong free_public_memory;
		public ulong total_private_memory;
		public ulong free_private_memory;
		public uint hardware_version_major;
		public uint hardware_version_minor;
		public uint firmware_version_major;
		public uint firmware_version_minor;
		public uint64 utc_time;
	}

	public class Slot : Glib.Object {
		public weak Module module { get; }
		public uint handle { get; }
		public SlotInfo info { get; }
		public TokenInfo token_info { get; }

		public GLib.List<uint> get_mechanisms();
		public MechanismInfo get_mechanism_info();
		
		[CCode (cname = "gp11_slot_open_session_full")]
		public Session open_session(uint flags, GLib.Cancellable cancellable = null) throws GLib.Error;
		public void open_session_async(uint flags, GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		public Session open_session_finish(GLib.AsyncResult result) throws GLib.Error;
	}

	[CCode (dup_function = "gp11_session_info_dup", free_function = "gp11_session_info_free")]
	public class SessionInfo {
		public ulong slot_id;
		public uint state;
		public uint flags;
		public ulong device_error;
	}

	public class Session : Glib.Object {
		public static Session from_handle(Slot slot, uint handle);

		public weak Module module { get; }
		public uint handle { get; }
		public SessionInfo info { get; }
		
		[CCode (cname = "gp11_session_login_full")]
		public void login(guint user_type, uchar[] pin, ulong pin_length, GLib.Cancellable cancellable = null) throws GLib.Error;
		public void login_async(guint user_type, uchar[] pin, ulong pin_length, GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		public void login_finish(GLib.AsyncResult result) throws GLib.Error;

		[CCode (cname = "gp11_session_logout_full")]
		public void logout(GLib.Cancellable cancellable = null) throws GLib.Error;
		public void logout_async(GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		public void logout_finish(GLib.AsyncResult result) throws GLib.Error;

		[CCode (cname = "gp11_session_create_object_full")]
		public Object create_object(Attributes attrs, GLib.Cancellable cancellable = null) throws GLib.Error;
		public void create_object_async(Attributes attrs, GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		public Object create_object_finish(GLib.AsyncResult result) throws GLib.Error;
		
		[CCode (cname = "gp11_session_find_objects_full")]
		public GLib.List<Object> find_objects(Attributes attrs, GLib.Cancellable cancellable = null) throws GLib.Error;
		public void find_objects_async(Attributes attrs, GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		public GLib.List<Object> find_objects_finish(GLib.AsyncResult result) throws GLib.Error;
	}

	public class Object : Glib.Object {
		public static Object from_handle(Session session, uint handle);
		
		public weak Module module { get; }
		public weak Session session { get; }
		public uint handle { get; }

		[CCode (cname = "gp11_object_destroy_full")]
		public void destroy(GLib.Cancellable cancellable = null) throws GLib.Error;
		public void destroy_async(GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		public void destroy_finish(GLib.AsyncResult result) throws GLib.Error;

		[CCode (cname = "gp11_object_set_full")]
		public void set_attributes(Attributes attrs, GLib.Cancellable cancellable = null) throws GLib.Error;
		[CCode (cname = "gp11_object_set_async")]
		public void set_attributes_async(Attributes attrs, GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		[CCode (cname = "gp11_object_set_finish")]
		public void set_attributes_finish(GLib.AsyncResult result) throws GLib.Error;

		[CCode (cname = "gp11_object_get_full")]
		public Attributes get_attributes(guint[] attr_types, GLib.Cancellable cancellable = null) throws GLib.Error;
		[CCode (cname = "gp11_object_get_async")]
		public void get_attributes_async(guint[] attr_types, GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		[CCode (cname = "gp11_object_get_finish")]
		public Attributes get_attributes_finish(GLib.AsyncResult result) throws GLib.Error;
		
		[CCode (cname = "gp11_object_get_one_full")]
		public Attribute get_attribute(guint attr_type, GLib.Cancellable cancellable = null) throws GLib.Error;
		[CCode (cname = "gp11_object_get_one_async")]
		public void get_attribute_async(guint attr_type, GLib.Cancellable cancellable, GLib.AsyncReadyCallback callback);
		[CCode (cname = "gp11_object_get_one_finish")]
		public Attribute get_attribute_finish(GLib.AsyncResult result) throws GLib.Error;
	}
}
