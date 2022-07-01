/*
 * Seahorse
 *
 * Copyright (C) 2018 Niels De Graef
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

/**
 * The KeyLengthChooser allows the user choose the amount of bits a key should
 * have (i.e. the strength of a key). Since the several types of keys (see
 * {@link Ssh.Algorithm}) all support several sizes in their own way (if at
 * all even), we need to adjust the UI to the chosen type.
 *
 * Please also refer to the man pages of ssh-keygen to know why some
 * some restrictions are added.
 */
public class Seahorse.Ssh.KeyLengthChooser : Adw.Bin {

    private Gtk.SpinButton spin_button;  // For RSA
    private Gtk.DropDown dropdown;   // For ECDSA
    private Gtk.Label not_supported;     // For DSA, ED25519 and unknowns

    public Algorithm algorithm { get; set; }

    public KeyLengthChooser(Algorithm algo = Algorithm.RSA) {
        var stack = new Gtk.Stack();
        this.child = stack;

        // RSA
        var rsa_size_adj = new Gtk.Adjustment(2048, 1024, 8193, 256, 10, 1);
        this.spin_button = new Gtk.SpinButton(rsa_size_adj, 1, 0);
        stack.add_child(this.spin_button);

        // ECDSA
        this.dropdown = new Gtk.DropDown.from_strings({ "256", "384", "521" });
        this.dropdown.selected = 0;
        stack.add_child(this.dropdown);

        // DSA, ED25519, and Unknown
        this.not_supported = new Gtk.Label(null);
        stack.add_child(this.not_supported);

        // Now set the initial algorithm
        this.notify["algorithm"].connect(on_algo_changed);
        this.algorithm = algo;
    }

    /**
     * Returns the chosen key length (in bits). In case of an unknown algorithm,
     * it will return 0.
     */
    public int get_length () {
        switch (this.algorithm) {
            case Algorithm.RSA:
                return this.spin_button.get_value_as_int();
            case Algorithm.DSA:
                return 1024;
            case Algorithm.ECDSA:
                var str = ((Gtk.StringObject) this.dropdown.selected_item).string;
                return int.parse(str);
            case Algorithm.ED25519:
                return 256;
            default:
                return 0;
        }
    }

    private void on_algo_changed (GLib.Object src, ParamSpec pspec) {
        unowned var stack = (Gtk.Stack) this.child;

        switch (this.algorithm) {
            case Algorithm.RSA:
                stack.visible_child = this.spin_button;
                break;
            case Algorithm.ECDSA:
                stack.visible_child = this.dropdown;
                break;
            case Algorithm.DSA:
                this.not_supported.label = _("1024 bits");
                stack.visible_child = this.not_supported;
                break;
            case Algorithm.ED25519:
                this.not_supported.label = _("256 bits");
                stack.visible_child = this.not_supported;
                break;
            case Algorithm.UNKNOWN:
                this.not_supported.label = _("Unknown key type!");
                stack.visible_child = this.not_supported;
                break;
        }
    }
}
