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

[GtkTemplate (ui = "/org/gnome/Seahorse/pkcs11-cert-key-pair-panel.ui")]
public class Seahorse.Pkcs11.CertKeyPairPanel : Seahorse.Panel {

    [GtkChild] private unowned Gtk.StackSwitcher stack_switcher;
    [GtkChild] private unowned Gtk.Stack stack;

    public Pkcs11.CertKeyPair pair { get; construct set; }

    private const ActionEntry[] ACTIONS = {
        { "export", action_export },
        { "request-certificate", action_request_certificate },
        { "delete", action_delete },
    };

    construct {
        this.title = this.pair.description;

        // Setup actions
        this.actions.add_action_entries(ACTIONS, this);
        insert_action_group("panel", this.actions);

        var action = (SimpleAction) this.actions.lookup_action("export");
        action.set_enabled(this.pair.exportable);
        action = (SimpleAction) this.actions.lookup_action("delete");
        action.set_enabled(this.pair.deletable);
        var req_cert_action = (SimpleAction) this.actions.lookup_action("request-certificate");
        req_cert_action.set_enabled(this.pair.private_key != null);

        if (this.pair.certificate != null) {
            var cert_widget = new Pkcs11.CertificateWidget(this.pair.certificate);
            cert_widget.notification.connect((c, toast) => { notification(toast); });
            this.stack.add_named(cert_widget, "certificate-page");
            this.stack.get_page(cert_widget).title = _("Certificate");
        }

        if (this.pair.private_key != null) {
            var key_widget = new Pkcs11.PrivateKeyWidget(this.pair.private_key);
            this.stack.add_named(key_widget, "private-key-page");
            this.stack.get_page(key_widget).title = _("Private key");

            this.pair.private_key.bind_property("certificate-request-capable",
                                                req_cert_action, "enabled",
                                                BindingFlags.SYNC_CREATE);
        }
    }

    public CertKeyPairPanel(Pkcs11.CertKeyPair pair) {
        GLib.Object(pair: pair);
    }

    public override void dispose() {
        unowned Gtk.Widget? child = null;
        while ((child = get_first_child()) != null)
            child.unparent();
        base.dispose();
    }

    private void action_export(SimpleAction action, Variant? param) {
        export_certificate_async.begin();
    }

    private async void export_certificate_async() {
        unowned var toplevel = get_root() as Gtk.Window;

        var export_op = this.pair.create_export_operation();
        try {
            var prompted = yield export_op.prompt_for_file(toplevel, null);
            if (!prompted) {
                debug("no file picked by user");
                return;
            }

            yield export_op.execute(null);
        } catch (GLib.IOError.CANCELLED e) {
            debug("Exporting of certificate cancelled by user");
        } catch (GLib.Error e) {
            Util.show_error(toplevel, _("Couldn’t export certificate"), e.message);
        }
    }

    private void action_delete(SimpleAction action, Variant? param) {
        unowned var toplevel = get_root() as Gtk.Window;

        var delete_op = new Pkcs11.DeleteOperation(this.pair);

        delete_op.execute_interactively.begin(toplevel, null, (obj, res) => {
            try {
                delete_op.execute_interactively.end(res);
            } catch (GLib.IOError.CANCELLED e) {
                debug("Delete cancelled by user");
            } catch (GLib.Error e) {
                Util.show_error(toplevel,
                                _("Couldn’t delete certificate / key pair"),
                                e.message);
            }
        });
    }

    private void action_request_certificate(SimpleAction action, Variant? param)
            requires(this.pair.private_key != null) {
        unowned var toplevel = get_root() as Gtk.Window;
        var req_dialog = new Pkcs11.RequestCertificateDialog(this.pair.private_key);
        req_dialog.present(toplevel);
    }
}
