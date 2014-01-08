/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2012 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

namespace Seahorse {

public class CertificateDerExporter : GLib.Object, Exporter {
	private Gcr.Certificate _certificate;
	private GLib.List<weak GLib.Object> _objects;

	public CertificateDerExporter(Gcr.Certificate certificate)
	{
		this._certificate = certificate;
		this._objects.append(certificate);
	}

	public string filename {
		owned get {
			string? label = null;
			if (this._certificate != null) {
				label = this._certificate.label;
				if (label == null)
					label = this._certificate.description;
			}
			if (label == null)
				label = _("Certificate");

			string filename = label + ".crt";
			return filename.delimit(BAD_FILENAME_CHARS, '_');
		}
	}

	public string content_type {
		get { return "application/pkix-cert"; }
	}

	public Gtk.FileFilter file_filter {
		owned get {
			var filter = new Gtk.FileFilter();
			filter.set_name(_("Certificates (DER encoded)"));
			filter.add_mime_type ("application/pkix-cert");
			filter.add_mime_type ("application/x-x509-ca-cert");
			filter.add_mime_type ("application/x-x509-user-cert");
			filter.add_pattern ("*.cer");
			filter.add_pattern ("*.crt");
			filter.add_pattern ("*.cert");

			return filter;

		}
	}

	public unowned GLib.List<weak GLib.Object> get_objects() {
		return this._objects;
	}

	public bool add_object(GLib.Object obj) {
		return false;
	}

	public async uchar[] export(GLib.Cancellable? cancellable)
			throws GLib.Error {
		return this._certificate.get_der_data();
	}

}

}
