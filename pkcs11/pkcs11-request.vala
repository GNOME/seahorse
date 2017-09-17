/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2013 Red Hat Inc.
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
 *
 * Stef Walter <stefw@redhat.com>
 */

namespace Seahorse {
namespace Pkcs11 {

public class Request : Gtk.Dialog {
	public PrivateKey private_key { construct; get; }

	Gtk.Entry _name_entry;
	uint8[] _encoded;

	construct {
		var builder = new Gtk.Builder();
		var path = "/org/gnome/Seahorse/seahorse-pkcs11-request.ui";
		try {
			builder.add_from_resource(path);
		} catch (GLib.Error err) {
			GLib.warning("couldn't load ui file: %s", path);
			return;
		}

		this.set_resizable(false);
		var content = this.get_content_area();
		var widget = (Gtk.Widget)builder.get_object("pkcs11-request");
		content.add(widget);
		widget.show();

		this._name_entry = (Gtk.Entry)builder.get_object("request-name");
		this._name_entry.changed.connect(() => { update_response(); });

		/* The buttons */
		this.add_buttons(Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
		                 _("Create"), Gtk.ResponseType.OK);
		this.set_default_response (Gtk.ResponseType.OK);

		this.update_response ();

		if (!(this.private_key is Gck.Object)) {
			GLib.critical("private key is not of type %s", typeof(Gck.Object).name());
		}
	}

	public static void prompt(Gtk.Window? parent,
	                          Gck.Object private_key) {
		var dialog = (Request)GLib.Object.new(typeof(Request), transient_for: parent,
		                                      private_key: private_key);
		dialog.run();
	}

	public override void response(int response_id) {
		if (response_id == Gtk.ResponseType.OK) {
			var interaction = new Interaction(this.transient_for);
			var session = this.private_key.get_session();
			session.set_interaction(interaction);

			var req = Gcr.CertificateRequest.prepare(Gcr.CertificateRequestFormat.CERTIFICATE_REQUEST_PKCS10,
			                                         this.private_key);
			req.set_cn(this._name_entry.get_text());
			req.complete_async.begin(null, (obj, res) => {
				try {
					req.complete_async.end(res);
					this.save_certificate_request(req, this.transient_for);
				} catch (GLib.Error err) {
					Util.show_error(this.transient_for, _("Couldn’t create certificate request"), err.message);
				}
			});

			this.hide();
		}
	}

	private void update_response() {
		string name = this._name_entry.get_text();
		this.set_response_sensitive(Gtk.ResponseType.OK, name != "");
	}

	private static string BAD_FILENAME_CHARS = "/\\<>|?*";

	private void save_certificate_request(Gcr.CertificateRequest req,
	                                      Gtk.Window? parent) {
		var chooser = new Gtk.FileChooserDialog(_("Save certificate request"),
		                                        parent, Gtk.FileChooserAction.SAVE,
		                                        Gtk.Stock.CANCEL, Gtk.ResponseType.CANCEL,
		                                        Gtk.Stock.SAVE, Gtk.ResponseType.ACCEPT);

		chooser.set_default_response(Gtk.ResponseType.ACCEPT);
		chooser.set_local_only(false);

		var der_filter = new Gtk.FileFilter();
		der_filter.set_name(_("Certificate request"));
		der_filter.add_mime_type("application/pkcs10");
		der_filter.add_pattern("*.p10");
		der_filter.add_pattern("*.csr");
		chooser.add_filter(der_filter);
		chooser.set_filter(der_filter);

		var pem_filter = new Gtk.FileFilter();
		pem_filter.set_name(_("PEM encoded request"));
		pem_filter.add_mime_type("application/pkcs10+pem");
		pem_filter.add_pattern("*.pem");
		chooser.add_filter(pem_filter);

		string? label;
		this.private_key.get("label", out label);
		if (label == null || label == "")
			label = "Certificate Request";
		var filename = label + ".csr";
		filename = filename.delimit(BAD_FILENAME_CHARS, '_');
		chooser.set_current_name(filename);

		chooser.set_do_overwrite_confirmation(true);

		var response = chooser.run();
		if (response == Gtk.ResponseType.ACCEPT) {
			bool textual = chooser.get_filter() == pem_filter;
			this._encoded = req.encode(textual);

			var file = chooser.get_file();
			file.replace_contents_async.begin(this._encoded, null, false,
			                                  GLib.FileCreateFlags.NONE,
			                                  null, (obj, res) => {
				try {
					string new_etag;
					file.replace_contents_async.end(res, out new_etag);
				} catch (GLib.Error err) {
					Util.show_error(parent, _("Couldn’t save certificate request"), err.message);
				}
			});
		}

		chooser.destroy();
	}
}

}
}
