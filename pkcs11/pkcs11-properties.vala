/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2013 Red Hat Inc.
 * Copyright (C) 2020 Niels De Graef
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
 * Author: Stef Walter <stefw@redhat.com>
 */

[GtkTemplate (ui = "/org/gnome/Seahorse/seahorse-pkcs11-properties.ui")]
public class Seahorse.Pkcs11.Properties : Gtk.Dialog {

    public Gck.Object object { construct; get; }

    /* [GtkChild] */
    /* private Gtk.HeaderBar header; */
    [GtkChild]
    private unowned Gtk.Button delete_button;
    [GtkChild]
    private unowned Gtk.Button export_button;
    [GtkChild]
    private unowned Gtk.Button request_certificate_button;

    [GtkChild]
    private unowned Gtk.Box content;

    private Gcr.Viewer _viewer;
    private GLib.Cancellable _cancellable;
    private Gck.Object _request_key;

    public Properties(Gck.Object object, Gtk.Window window) {
        GLib.Object(object: object, transient_for: window);
    }

    construct {
        this._cancellable = new GLib.Cancellable();

        this._viewer = Gcr.Viewer.new_scrolled();
        this.content.pack_start(this._viewer);
        this._viewer.set_hexpand(true);
        this._viewer.set_vexpand(true);
        this._viewer.show();

        /* ... */

        this.object.bind_property("deletable", this.delete_button, "visible",
                                  BindingFlags.SYNC_CREATE);
        this.object.bind_property("exportable", this.export_button, "visible",
                                  BindingFlags.SYNC_CREATE);

        object.notify["label"].connect(() => { update_label(); });
        update_label();
        add_renderer_for_object(this.object);
        check_certificate_request_capable(this.object);

        GLib.Object? partner;
        this.object.get("partner", out partner);
        if (partner != null) {
            add_renderer_for_object(partner);
            check_certificate_request_capable(partner);
        }

        GLib.List<Exporter> exporters = null;
        if (this.object is Exportable)
            exporters = ((Exportable)this.object).create_exporters(ExporterType.ANY);

        this.export_button.set_visible(exporters != null);

        this._viewer.grab_focus();
    }

    public override void dispose() {
        this._cancellable.cancel();
        base.dispose();
    }

    private void update_label() {
        string? label;
        string? description;
        this.object.get("label", out label, "description", out description);
        if (label == null || label == "")
            label = _("Unnamed");
        this.title = "%s - %s".printf(label, description);
    }

    private void add_renderer_for_object(GLib.Object object) {
        Gck.Attributes? attributes = null;
        string? label = null;

        object.get("label", &label, "attributes", &attributes);
        if (attributes != null) {
            var renderer = Gcr.Renderer.create(label, attributes);
            if (renderer != null) {
                object.bind_property("label", renderer, "label",
                                     GLib.BindingFlags.DEFAULT);
                object.bind_property("attributes", renderer, "attributes",
                                     GLib.BindingFlags.DEFAULT);

                if (renderer.get_class().find_property("object") != null)
                    renderer.set("object", object);

                this._viewer.add_renderer(renderer);
            }
        }
    }

    [GtkCallback]
    private void on_export_button_clicked(Gtk.Button export_button) {
        GLib.List<GLib.Object> objects = null;
        objects.append(this.object);
        try {
            Exportable.export_to_prompt_wait(objects, this);
        } catch (GLib.Error err) {
            Util.show_error(this, _("Failed to export certificate"), err.message);
        }
    }

    [GtkCallback]
    private void on_delete_button_clicked(Gtk.Button delete_button) {
        GLib.Object? partner;
        this.object.get("partner", out partner);

        Deleter deleter;
        if (partner != null || this.object is PrivateKey) {
            deleter = new KeyDeleter((Gck.Object)this.object);
            if (!deleter.add_object(partner))
                GLib.assert_not_reached();
        } else {
            deleter = new Deleter((Gck.Object)this.object);
        }

        if (deleter.prompt(this)) {
            deleter.delete.begin(this._cancellable, (obj, res) => {
                try {
                    if (deleter.delete.end(res))
                        this.destroy();
                } catch (GLib.Error err) {
                    Util.show_error(this, _("Couldn’t delete"), err.message);
                }
            });
        }
    }

    [GtkCallback]
    private void on_request_certificate_button_clicked(Gtk.Button request_button) {
        Request.prompt(this, this._request_key);
    }

    private void check_certificate_request_capable(GLib.Object object) {
        if (!(object is PrivateKey))
            return;

        Gcr.CertificateRequest.capable_async.begin((PrivateKey)object, this._cancellable, (obj, res) => {
            try {
                if (Gcr.CertificateRequest.capable_async.end(res)) {
                    this.request_certificate_button.set_visible(true);
                    this._request_key = (PrivateKey)object;
                }
            } catch (GLib.Error err) {
                GLib.message("couldn't check capabilities of private key: %s", err.message);
            }
        });
    }
}
