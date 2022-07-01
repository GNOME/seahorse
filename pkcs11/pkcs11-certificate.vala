/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2013 Red Hat, Inc.
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

public class Seahorse.Pkcs11.Certificate : Gck.Object, Gcr.Certificate, Gck.ObjectCache {

    private Gck.Attributes? _attributes;
    public Gck.Attributes attributes {
        owned get { return this._attributes; }
        set {
            this._attributes = value;
            this.notify_property("attributes");
        }
    }

    public bool exportable {
        get { return this._der != null; }
    }

    public string? subject_name {
        owned get { return get_subject_name(); }
    }

    public string? issuer_name {
        owned get { return get_issuer_name(); }
    }

    public GLib.DateTime? expiry_date {
        owned get { return get_expiry_date(); }
    }

    public string description {
        owned get { return _("Certificate"); }
    }

    public string? label {
        owned get { return get_subject_name(); }
    }

    private unowned Gck.Attribute? _der = null;

    private static uint8[] EMPTY = { };

    construct {
        this.notify["attributes"].connect((pspec) => {
            if (this._attributes != null)
                this._der = this._attributes.find(CKA.VALUE);
            notify_property ("subject-name");
            notify_property ("issuer-name");
            notify_property ("expiry-date");
        });

        if (this._attributes != null)
            this._der = this._attributes.find(CKA.VALUE);
    }

    public void fill(Gck.Attributes attributes) {
        var builder = new Gck.Builder(Gck.BuilderFlags.NONE);

        if (this._attributes != null)
            builder.add_all(this._attributes);
        builder.set_all(attributes);
        this._attributes = builder.end();
        this.notify_property("attributes");
    }

    [CCode (array_length_type = "gsize")]
    public unowned uint8[] get_der_data() {
        if (this._der == null)
            return EMPTY;
        return this._der.get_data();
    }
}
