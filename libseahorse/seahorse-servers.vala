
namespace Seahorse {

	public static delegate bool ValidUriFunc (string uri);
	
	public class Servers : GLib.Object {
		private class ServerInfo {
			public string type;
			public string description;
			public ValidUriFunc validator;
		}
		
		private static HashTable<string, ServerInfo> types;
		
		static construct {
			/* TODO: What do we specify to free ServerInfo? */
			types = new HashTable<string, ServerInfo>.full(str_hash, str_equal, g_free, null);
		}
		
		public static SList<string> get_types() {
			SList<string> results;
			foreach (var type in types.get_keys())
				results.append(type);
			return results;
		}
		
		public static string? get_description(string type) {
			var server = types.lookup(type);
			if (server == null)
				return null;
			return server.description;
		}
		
		public static void register_type(string type, string description, ValidUriFunc validate) {
			/* Work around for: http://bugzilla.gnome.org/show_bug.cgi?id=543190 */
			Servers dummy = new Servers();

			ServerInfo info = new ServerInfo();
			info.type = type;
			info.description = description;
			info.validator = validate;
			types.replace(type, info);
		}
		
		public static GLib.SList<string> get_uris() {
			var servers = Conf.get_string_list (Conf.KEYSERVER_KEY);
			GLib.SList<string> results;
			
			/* The values are 'uri name', remove the name part */
			foreach (string value in servers) {
				string[] parts = value.split(" ", 2);
				results.append(parts[0]);
			}
			
			return results;
		}
		
		public static GLib.SList<string> get_names() {
			var servers = Conf.get_string_list (Conf.KEYSERVER_KEY);
			GLib.SList<string> results;
			
			/* The values are 'uri name', remove the uri part */
			foreach (string value in servers) {
				string[] parts = value.split(" ", 2);
				if (parts.length == 2 && parts[1].len() > 0)
					results.append(parts[1]);
				else
					results.append(parts[0]);
			}
			
			return results;
		}
		
		/* Check to see if the passed uri is valid against registered validators */
		public static bool is_valid_uri(string uri) {
			string[] parts = uri.split(":", 2);
			if(parts.length == 0)
				return false;
			ServerInfo? info = types.lookup(parts[0]);
			if(info == null)
				return false;
			return info.validator (uri);
		}
	}
}