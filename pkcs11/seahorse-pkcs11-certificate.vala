
namespace Seahorse.Pkcs11 {
	public class Certificate : Seahorse.Object {

		private GP11.Object? _pkcs11_object;
		public GP11.Object pkcs11_object {
			get { return _pkcs11_object; }
			set { _pkcs11_object = value; }
		}
		
		private GP11.Attributes? _pkcs11_attributes;
		public GP11.Attributes pkcs11_attributes {
			get { return _pkcs11_attributes; }
			set { _pkcs11_attributes = value; rebuild(); }
		}
		
		public override string# display_name {
			get { 
				if (_pkcs11_attributes != null) {
					string? label;
					if (_pkcs11_attributes.find_string(P11.CKA_LABEL, out label)) {
						if (label != null)
							return label;
					}
				}
				
				/* TODO: Calculate something from the subject? */
				return _("Certificate");
			}
		}
		
		public string# display_id {
			get { 
				string id = fingerprint;
				if(id.len() <= 8)
					return id;
				return id.substring(id.len() - 8, 8);
			}
		}

		public override string# markup {
			get { return GLib.Markup.escape_text(display_name); }
		}
		
		public string simple_name {
			get { return display_name; }
		}
		
		public string# fingerprint {
			get {
				/* TODO: We should be using the fingerprint off the key */
				if (_pkcs11_attributes == null)
					return "";
				weak GP11.Attribute? attr = _pkcs11_attributes.find(P11.CKA_ID);
				if (attr == null)
					return "";
				return Util.hex_encode(attr.value, attr.length);
			}
		}
		
		public int validity { 
			get { 
				/* TODO: We need to implement proper validity checking */; 
				return Validity.UNKNOWN;
			}
		}
		
		public string# validity_str {
			get { return Util.validity_to_string((Seahorse.Validity)validity); }
		}

		public int trust {
			get { 
				ulong trust;
				if (_pkcs11_attributes == null ||
				    !_pkcs11_attributes.find_ulong(P11.CKA_GNOME_USER_TRUST, out trust))
					return Validity.UNKNOWN;
				if (trust == P11.CKT_GNOME_TRUSTED)
					return Validity.FULL;
				else if (trust == P11.CKT_GNOME_UNTRUSTED)
					return Validity.NEVER;
				return Validity.UNKNOWN;
			}
		}
		
		public string# trust_str {
			get { return Util.validity_to_string((Seahorse.Validity)trust); }
		}

		public ulong expires {
			get { 
				GLib.Date date;
				if (_pkcs11_attributes == null ||
				    !_pkcs11_attributes.find_date(P11.CKA_END_DATE, out date))
					return 0;
				Time time;
				date.to_time(out time);
				return (ulong)time.mktime();
			}
		}
		
		public string# expires_str {
			get { 	
				/* TODO: When expired return Expired */
				ulong expiry = expires;
				if (expiry == 0)
					return "";
				return Util.get_date_string(expiry);
			}
		}

		public override weak string# stock_id {
			get {
				/* TODO: A certificate icon */
				return ""; 
			}
		}
		
		public Certificate(GP11.Object object, GP11.Attributes attributes) {
			this.pkcs11_object = object;
			this.pkcs11_attributes = attributes;
		}
		
		private void rebuild() {
			_id = 0;
			_tag = Pkcs11.TYPE;
			
			if (_pkcs11_attributes == null) {
				_location = Location.INVALID;
				_usage = Usage.NONE;
				_flags = Key.Flag.DISABLED;
			} else {
				_id = id_from_attributes(_pkcs11_attributes);
				_location = Location.LOCAL;
				_usage = Usage.PUBLIC_KEY;
				_flags = 0;
				
				/* TODO: Expiry, revoked, disabled etc... */
				
				if (trust >= (int)Validity.MARGINAL)
					_flags |= Key.Flag.TRUSTED;
			}
			
			fire_changed(Change.ALL);
		}
	}
}
