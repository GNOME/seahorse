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
using GP11;

namespace Seahorse.Pkcs11 {
	
	public const string TYPE_STR = "pkcs11";
	public const Quark TYPE = Quark.from_string("pkcs11");
	
	public GLib.Quark id_from_attributes (GP11.Attributes attrs) {
		
		/* These cases should have been covered by the programmer */
		
		ulong klass;
		if (!attrs.find_ulong(P11.CKA_CLASS, out klass)) {
			GLib.warning("Cannot create object id. PKCS#11 attribute set did not contain CKA_CLASS.");
			return 0;
		}

		weak GP11.Attribute? attr = attrs.find(P11.CKA_ID);
		if (attr == null) {
			GLib.warning("Cannot create object id. PKCS#11 attribute set did not contain CKA_ID");
			return 0;
		}

		string value = "%s:%s/%s".printf(TYPE_STR, klass_to_string(klass), Bugs.base64_encode(attr.value, attr.length));
		return Quark.from_string(value);
	}
	
	public bool id_to_attributes (GLib.Quark id, GP11.Attributes attrs) {
		if(id == 0)
			return false;
		
		weak string value = id.to_string(); 
		string[] parts = value.split_set(":/", 3);
		if (parts.length != 3 || parts[0] != TYPE_STR)
			return false;
		
		ulong klass = string_to_klass(parts[1]);
		if (klass == -1)
			return false;
		
		size_t len;
		uchar[] ckid = GLib.Base64.decode(parts[2], out len);
		if (ckid == null)
			return false;
		
		attrs.add_data(P11.CKA_ID, ckid);
		attrs.add_ulong(P11.CKA_CLASS, klass);
		return true;
	}
	
	private string klass_to_string(ulong klass) {
		switch (klass) {
		case P11.CKO_DATA:
			return "data";
		case P11.CKO_CERTIFICATE:
			return "certificate";
		case P11.CKO_PRIVATE_KEY:
			return "private-key";
		case P11.CKO_PUBLIC_KEY:
			return "public-key";
		default:
			return "%lu".printf(klass);
		}
	}
	
	private ulong string_to_klass(string str) {
		if (str == "data")
			return P11.CKO_DATA;
		else if (str == "certificate")
			return P11.CKO_CERTIFICATE;
		else if (str == "private-key")
			return P11.CKO_PRIVATE_KEY;
		else if (str == "public-key")
			return P11.CKO_PUBLIC_KEY;
		else {
			string end;
			ulong ret = str.to_ulong(out end);
			if (end.len() > 0) {
				GLib.warning("unrecognized and unparsable PKCS#11 class: %s", str);
				return -1;
			}
			return ret;
		}
	}
	
}
