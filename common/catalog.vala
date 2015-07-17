/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

namespace Seahorse {

public abstract class Catalog : Gtk.Window {
	public static const string MENU_OBJECT = "ObjectPopup";

	/* For compatibility with old code */
	public Gtk.Window window {
		get { return this; }
	}

	/* Set by the derived classes */
	public string ui_name { construct; get; }

	private Gtk.Builder _builder;
	private Gtk.UIManager _ui_manager;
	private GLib.HashTable<Gtk.ActionGroup, weak Gtk.ActionGroup> _actions;
	private Gtk.Action _edit_delete;
	private Gtk.Action _properties_object;
	private Gtk.Action _file_export;
	private Gtk.Action _edit_copy;
	private GLib.List<Gtk.ActionGroup> _selection_actions;
	private bool _disposed;
	private GLib.Settings _settings;

	public abstract GLib.List<weak Backend> get_backends();
	public abstract Place? get_focused_place();
	public abstract GLib.List<weak GLib.Object> get_selected_objects();

	construct {
		this._builder = Util.load_built_contents(this, this.ui_name);

		this._actions = new GLib.HashTable<Gtk.ActionGroup, weak Gtk.ActionGroup>(GLib.direct_hash, GLib.direct_equal);
		this._ui_manager = new Gtk.UIManager();

		this._ui_manager.add_widget.connect((widget) => {
			unowned string? name;
			if (widget is Gtk.MenuBar)
				name = "menu-placeholder";
			else if (widget is Gtk.Toolbar)
				name = "toolbar-placeholder";
			else
				name = null;
			var holder = this._builder.get_object(name);
			if (holder != null) {
				((Gtk.Container)holder).add(widget);
				widget.show();
			} else {
				GLib.warning ("no place holder found for: %s", name);
			}
		});

		this._ui_manager.pre_activate.connect((action) => {
			Action.pre_activate(action, this, this);
		});

		this._ui_manager.post_activate.connect((action) => {
			Action.post_activate(action);
		});

		/* Load window size for windows that aren't dialogs */
		var key = "/apps/seahorse/windows/%s/".printf(this.ui_name);
		this._settings = new GLib.Settings.with_path("org.gnome.seahorse.window", key);
		var width = this._settings.get_int("width");
		var height = this._settings.get_int("height");
		if (width > 0 && height > 0)
			this.resize (width, height);

		/* The widgts get added in an idle loop later */
		try {
			var path = "/org/gnome/Seahorse/seahorse-%s.ui".printf(this.ui_name);
			this._ui_manager.add_ui_from_resource(path);
		} catch (GLib.Error err) {
			GLib.warning("couldn't load ui description for '%s': %s",
			             this.ui_name, err.message);
		}

		this.add_accel_group (this._ui_manager.get_accel_group());

		var actions = new Gtk.ActionGroup("main");
		actions.set_translation_domain(Config.GETTEXT_PACKAGE);
		actions.add_actions(UI_ENTRIES, this);

		var action = actions.get_action("app-preferences");
		action.set_visible (Prefs.available());
		this._edit_delete = actions.get_action("edit-delete");
		this._properties_object = actions.get_action("properties-object");
		this._edit_copy = actions.get_action("edit-export-clipboard");
		this._file_export = actions.get_action("file-export");
		this._ui_manager.insert_action_group (actions, 0);

		Seahorse.Application.get().add_window(this);
	}

	public override void dispose() {
		this._edit_copy = null;
		this._edit_delete = null;
		this._file_export = null;
		this._properties_object = null;

		foreach (var group in this._selection_actions)
			this._ui_manager.remove_action_group(group);
		this._selection_actions = null;

		this._ui_manager = null;
		this._actions.remove_all();

		if (!this._disposed) {
			this._disposed = true;

			int width, height;
			this.get_size(out width, out height);
			this._settings.set_int("width", width);
			this._settings.set_int("height", height);
			Seahorse.Application.get().remove_window (this);
		}

		base.dispose();
	}

	public virtual signal void selection_changed() {
		bool can_properties = false;
		bool can_delete = false;
		bool can_export = false;

		var objects = this.get_selected_objects();
		foreach (var object in objects) {
			if (Exportable.can_export(object))
				can_export = true;
			if (Deletable.can_delete(object))
				can_delete = true;
			if (Viewable.can_view(object))
				can_properties = true;
			if (can_export && can_delete && can_properties)
				break;
		}

		this._properties_object.sensitive = can_properties;
		this._edit_delete.sensitive = can_delete;
		this._edit_copy.sensitive = can_export;
		this._file_export.sensitive = can_export;

		foreach (var group in this._selection_actions)
			group.visible = false;
		this._selection_actions = lookup_actions_for_objects(objects);
		foreach (var group in this._selection_actions)
			group.visible = true;
	}

	public unowned Gtk.Builder get_builder() {
		return this._builder;
	}

	public unowned T? get_widget<T>(string name) {
		return (T)this._builder.get_object(name);
	}

	public void ensure_updated() {
		this._ui_manager.ensure_update();
	}

	public void include_actions(Gtk.ActionGroup group) {
		this._ui_manager.insert_action_group(group, 10);

		if (group is Actions) {
			var actions = (Actions)group;
			actions.catalog = this;

			var definition = actions.definition;
			if (definition != null) {
				try {
					this._ui_manager.add_ui_from_string (definition, -1);
				} catch (GLib.Error err) {
					GLib.warning ("couldn't add ui defintion for action group: %s: %s",
					              actions.name, definition);
				}
			}
		}

		this._actions.add(group);
	}

	public void show_properties(GLib.Object obj) {
		Viewable.view(obj, this);
	}

	public void show_context_menu(string name,
	                              uint button,
	                              uint time)
	{
		var path = "/%s".printf(name);
		var menu = this._ui_manager.get_widget(path);

		if (menu == null)
			return;
		if (menu is Gtk.Menu) {
			((Gtk.Menu)menu).popup(null, null, null, button, time);
			menu.show();
		} else {
			GLib.warning("the object /%s isn't a menu", name);
		}
	}

	private GLib.List<Gtk.ActionGroup> lookup_actions_for_objects (GLib.List<GLib.Object> objects) {
		var table = new GLib.HashTable<Gtk.ActionGroup, weak Gtk.ActionGroup>(GLib.direct_hash, GLib.direct_equal);
		foreach (var object in objects) {
			Gtk.ActionGroup? actions = null;
			object.get("actions", out actions, null);
			if (actions == null)
				continue;
			if (this._actions.lookup(actions) == null)
				this.include_actions(actions);
			this._actions.add(actions);
		}

		var iter = GLib.HashTableIter<Gtk.ActionGroup, weak Gtk.ActionGroup>(table);
		var results = new GLib.List<Gtk.ActionGroup>();
		Gtk.ActionGroup group;
		while (iter.next(out group, null))
			results.prepend(group);

		return results;
	}

	[CCode (instance_pos = -1)]
	private void on_app_preferences (Gtk.Action action)
	{
		Prefs.show(this, null);
	}

	private static const string[] AUTHORS = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Jose Carlos Garcia Sogo <jsogo@users.sourceforge.net>",
		"Jean Schurger <yshark@schurger.org>",
		"Stef Walter <stef@memberwebs.com>",
		"Adam Schreiber <sadam@clemson.edu>",
		"",
		N_("Contributions:"),
		"Albrecht Dreß <albrecht.dress@arcor.de>",
		"Jim Pharis <binbrain@gmail.com>",
		null
	};

	private static const string[] DOCUMENTERS = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Adam Schreiber <sadam@clemson.edu>",
		"Milo Casagrande <milo_casagrande@yahoo.it>",
		null
	};

	private static const string[] ARTISTS = {
		"Jacob Perkins <jap1@users.sourceforge.net>",
		"Stef Walter <stef@memberwebs.com>",
		null
	};

	[CCode (instance_pos = -1)]
	private void on_app_about(Gtk.Action action)
	{

		var about = new Gtk.AboutDialog();
		about.set_artists(ARTISTS);
		about.set_authors(AUTHORS);
		about.set_documenters(DOCUMENTERS);
		about.set_version(Config.VERSION);
		about.set_comments(_("Passwords and Keys"));
		about.set_copyright("Copyright \xc2\xa9 2002 - 2010 Seahorse Project");
		about.set_translator_credits(_("translator-credits"));
		about.set_logo_icon_name("seahorse");
		about.set_website("http://www.gnome.org/projects/seahorse");
		about.set_website_label(_("Seahorse Project Homepage"));

		about.response.connect((response) => {
			about.hide();
		});

		about.set_transient_for(this);
		about.run();
		about.destroy();
	}

	[CCode (instance_pos = -1)]
	private void on_object_delete(Gtk.Action action)
	{
		try {
			var objects = this.get_selected_objects();
			Deletable.delete_with_prompt_wait(objects, this);
		} catch (GLib.Error err) {
			Util.show_error(window, _("Cannot delete"), err.message);
		}
	}

	[CCode (instance_pos = -1)]
	private void on_properties_object(Gtk.Action action) {
		var objects = get_selected_objects();
		if (objects.length() > 0)
			this.show_properties(objects.data);
	}

	[CCode (instance_pos = -1)]
	private void on_properties_place (Gtk.Action action) {
		var place = this.get_focused_place ();
		if (place != null)
			this.show_properties (place);
	}

	[CCode (instance_pos = -1)]
	private void on_key_export_file (Gtk.Action action) {
		try {
			Exportable.export_to_prompt_wait(this.get_selected_objects(), this);
		} catch (GLib.Error err) {
			Util.show_error(window, _("Couldn't export keys"), err.message);
		}
	}

	[CCode (instance_pos = -1)]
	private void on_key_export_clipboard (Gtk.Action action) {
		uint8[] output;
		try {
			var objects = this.get_selected_objects ();
			Exportable.export_to_text_wait (objects, out output);
		} catch (GLib.Error err) {
			Util.show_error(this, _("Couldn't export data"), err.message);
			return;
		}

		/* TODO: Print message if only partially exported */

		var board = Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD);
		board.set_text ((string)output, output.length);
	}

	[CCode (instance_pos = -1)]
	private void on_help_show(Gtk.Action action) {
		try {
			var document = "help:%s".printf(Config.PACKAGE);
			GLib.AppInfo.launch_default_for_uri(document, null);
		} catch (GLib.Error err) {
			Util.show_error(this, _("Could not display help: %s"), err.message);
		}
	}

	private static const Gtk.ActionEntry[] UI_ENTRIES = {
		/* Top menu items */
		{ "file-menu", null, N_("_File") },
		{ "file-export", Gtk.Stock.SAVE_AS, N_("E_xport..."), null,
		  N_("Export to a file"), on_key_export_file },
		{ "edit-menu", null, N_("_Edit") },
		/*Translators: This text refers to deleting an item from its type's backing store*/
		{ "edit-export-clipboard", Gtk.Stock.COPY, null, "<control>C",
		  N_("Copy to the clipboard"), on_key_export_clipboard },
		{ "edit-delete", Gtk.Stock.DELETE, N_("_Delete"), null,
		  N_("Delete selected items"), on_object_delete },
		{ "properties-object", Gtk.Stock.PROPERTIES, null, null,
		  N_("Show the properties of this item"), on_properties_object },
		{ "properties-keyring", Gtk.Stock.PROPERTIES, null, null,
		  N_("Show the properties of this keyring"), on_properties_place },
		{ "app-preferences", Gtk.Stock.PREFERENCES, N_("Prefere_nces"), null,
		  N_("Change preferences for this program"), on_app_preferences },
		{ "view-menu", null, N_("_View") },
		{ "help-menu", null, N_("_Help") },
		{ "app-about", Gtk.Stock.ABOUT, null, null,
		  N_("About this program"), on_app_about },
		{ "help-show", Gtk.Stock.HELP, N_("_Contents"), "F1",
		  N_("Show Seahorse help"), on_help_show }
	};


}

}
