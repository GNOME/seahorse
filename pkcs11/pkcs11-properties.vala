/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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
 * Author: Stef Walter <stefw@redhat.com>
 */

namespace Seahorse {
namespace Pkcs11 {

public class Properties : Gtk.Window {
	public Gck.Object object { construct; get; }

	private static string UI_STRING =
		"""<ui>
			<toolbar name='Toolbar'>
				<toolitem action='export-object'/>
				<toolitem action='delete-object'/>
				<separator name='MiddleSeparator' expand='true'/>
				<toolitem action='request-certificate'/>
			</toolbar>
		</ui>""";

	private Gtk.Box _content;
	private Gcr.Viewer _viewer;
	private GLib.Cancellable _cancellable;
	private Gck.Object _request_key;
	private Gtk.UIManager _ui_manager;
	private Gtk.ActionGroup _actions;

	public Properties(Gck.Object object,
	                  Gtk.Window window) {
		GLib.Object(object: object, transient_for: window);
	}

	construct {
		this._cancellable = new GLib.Cancellable();
		set_default_size (400, 400);

		this._content = new Gtk.Box(Gtk.Orientation.VERTICAL, 0);
		this.add(this._content);
		this._content.show();

		this._viewer = Gcr.Viewer.new_scrolled();
		this._content.add(this._viewer);
		this._viewer.set_hexpand(true);
		this._viewer.set_vexpand(true);
		this._viewer.show();

		/* ... */

		this._actions = new Gtk.ActionGroup("Pkcs11Actions");
		this._actions.set_translation_domain(Config.GETTEXT_PACKAGE);
		this._actions.add_actions(UI_ACTIONS, this);
		var action = this._actions.get_action("delete-object");
		this.object.bind_property("deletable", action, "sensitive",
		                          GLib.BindingFlags.SYNC_CREATE);
		action = this._actions.get_action("export-object");
		this.object.bind_property("exportable", action, "sensitive",
		                          GLib.BindingFlags.SYNC_CREATE);
		var request = this._actions.get_action("request-certificate");
		request.is_important = true;
		request.visible = false;

		this._ui_manager = new Gtk.UIManager();
		this._ui_manager.insert_action_group(this._actions, 0);
		this._ui_manager.add_widget.connect((widget) => {
			if (!(widget is Gtk.Toolbar))
				return;

			this._content.pack_start(widget, false, true, 0);
			this._content.reorder_child(widget, 0);

			widget.get_style_context().add_class(Gtk.STYLE_CLASS_PRIMARY_TOOLBAR);
			widget.reset_style();
			widget.show();
		});
		try {
			this._ui_manager.add_ui_from_string(UI_STRING, -1);
		} catch (GLib.Error err) {
			GLib.critical ("%s", err.message);
		}
		this._ui_manager.ensure_update();

		this.object.notify["label"].connect(() => { this.update_label(); });
		this.update_label();
		this.add_renderer_for_object(this.object);
		this.check_certificate_request_capable(this.object);

		GLib.Object? partner;
		this.object.get("partner", out partner);
		if (partner != null) {
			this.add_renderer_for_object(partner);
			this.check_certificate_request_capable(partner);
		}

		GLib.List<Exporter> exporters;
		if (this.object is Exportable)
			exporters = ((Exportable)this.object).create_exporters(ExporterType.ANY);

		var export = this._actions.get_action("export-object");
		export.set_visible(exporters != null);

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
		this.set_title("%s - %s".printf(label, description));
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

	private void on_export_certificate(Gtk.Action action) {
		GLib.List<GLib.Object> objects = null;
		objects.append(this.object);
		try {
			Exportable.export_to_prompt_wait(objects, this);
		} catch (GLib.Error err) {
			Util.show_error(this, _("Failed to export certificate"), err.message);
		}
	}

	private void on_delete_objects(Gtk.Action action) {
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
					Util.show_error(this, _("Couldn't delete"), err.message);
				}
			});
		}
	}

	private void on_request_certificate(Gtk.Action action) {
		Request.prompt(this, this._request_key);
	}

	private static const Gtk.ActionEntry[] UI_ACTIONS = {
		{ "export-object", Gtk.Stock.SAVE_AS, N_("_Export"), "",
		  N_("Export the certificate"), on_export_certificate },
		{ "delete-object", Gtk.Stock.DELETE, N_("_Delete"), "<Ctrl>Delete",
		  N_("Delete this certificate or key"), on_delete_objects },
		{ "request-certificate", null, N_("Request _Certificate"), null,
		  N_("Create a certificate request file for this key"), on_request_certificate },
	};

	private void check_certificate_request_capable(GLib.Object object) {
		if (!(object is PrivateKey))
			return;

		Gcr.CertificateRequest.capable_async.begin((PrivateKey)object, this._cancellable, (obj, res) => {
			try {
				if (Gcr.CertificateRequest.capable_async.end(res)) {
					var request = this._actions.get_action("request-certificate");
					request.set_visible(true);
					this._request_key = (PrivateKey)object;
				}
			} catch (GLib.Error err) {
				GLib.message("couldn't check capabilities of private key: %s", err.message);
			}
		});
	}
}

}
}
