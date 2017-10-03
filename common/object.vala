/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2017 Niels De Graef
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

public class Seahorse.Object : GLib.Object {

/**
 * @label: DBUS: "display-name"
 * @markup: Markup text
 * @nickname: DBUS: "simple-name"
 * @identifier: DBUS: "key-id", "display-id", "raw-id"
 * @identifier_explicit:
 * @location: describes the loaction of the object (local, remte, invalid...)
 * @usage: DBUS: "etype"
 */


    // XXX only notify if changed
    /**
     * The place this Object came from.
     */
    [Notify]
    public weak Place place { get; set; default = null; }

    /**
     * Actions for the object
     */
    public Gtk.ActionGroup? actions { get; set; default = null; }

    /**
     * Stock ID for this Object.
     * XXX Nullable?
     */
    [Notify]
    public GLib.Icon? icon { get; set; default = new ThemedIcon("gtk-missing-image"); }

    /**
     * This object's displayable label.
     */
    [Notify]
    public string label {
        get { return this._label; }
        set {
            this._label = value;
            recalculate_label();
        }
    }
    private string _label = "";

    /**
     * This object's displayable markup.
     */
    // XXX explicit op true zetten in set;
    [Notify]
    public string markup {
        get { return this._markup; }
        set {
            this.markup_explicit = true;
            this._markup = value;
        }
    }
    private string _markup = "";
    // If true the markup will not be set automatically
    private bool markup_explicit;


    /**
     * This object's short name.
     */
    // XXX explicit op true zetten in set;
    [Notify]
    public string nickname {
        get { return this._nickname; }
        set {
            this.nickname_explicit = true;
            this._nickname = value;
        }
    }
    private string _nickname = "";
    // If true the nickname will not be set automatically
    private bool nickname_explicit;


    /**
     * Displayable ID for the object.
     */
    // XXX explicit op true zetten in set;
    [Notify]
    public string identifier {
        get { return this._identifier; }
        set {
            this._identifier = value;
        }
    }
    private string _identifier = "";

    // XXX only notify if changed
    /**
     * How this object is used.
     */
    [Notify]
    public Usage usage { get; set; default = Usage.NONE; }

    // XXX maybe we can set the property name with CCode?
    /**
     * This object's flags.
     */
    [Notify]
    public Flags object_flags { get; set; default = Flags.NONE; }

    /**
     * Whether this Object can be deleted.
     */
    public bool deletable { get { return Flags.DELETABLE in this.object_flags; } }

    /**
     * Whether this Object can be exported.
     */
    public bool exportable { get { return Flags.EXPORTABLE in this.object_flags; } }

    public Object() {
    }

    public Flags get_flags() {
        return this.object_flags;
    }

    public void set_flags(Flags flags) {
        this.object_flags = flags;
    }

    // Recalculates nickname and markup from the label
    private void recalculate_label() {
        if (!this.markup_explicit) {
            this.markup = Markup.escape_text (this.label ?? "");
            notify_property("markup");
        }

        if (!this.nickname_explicit) {
            this.nickname = this.label ?? "";
            notify_property("nickname");
        }
    }
}
