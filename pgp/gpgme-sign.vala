/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2006 Stefan Walter
 * Copyright (C) 2017 Niels De Graef
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

public class Seahorse.GpgME.Sign : Gtk.Dialog {

    private Seahorse.Object to_sign;

    private Gtk.RadioButton sign_choice_not;
    private Gtk.RadioButton sign_choice_casual;
    private Gtk.RadioButton sign_choice_careful;
    private Gtk.Label sign_display_not;
    private Gtk.Label sign_display_casual;
    private Gtk.Label sign_display_careful;
    private Gtk.CheckButton sign_option_local;
    private Gtk.CheckButton sign_option_revocable;
    private Gtk.ComboBox signer_select;
    private Gtk.Box signer_frame;
    private Gtk.Image sign_image;
    private Gtk.Label sign_uid_text;

    static bool sign_ok_clicked(GtkWindow *parent) {
        SeahorseSignOptions options = 0;

        // Figure out choice
        SignCheck check = SIGN_CHECK_NO_ANSWER;
        if (this.sign_choice_not.active) {
            check = SIGN_CHECK_NONE;
        } else {
            if (this.sign_choice_casual.active) {
                check = SIGN_CHECK_CASUAL;
            } else {
                if (this.sign_choice_careful.active)
                    check = SIGN_CHECK_CAREFUL;
            }
        }

        // Local signature
        if (this.sign_option_local.active)
            options |= SIGN_LOCAL;

        // Revocable signature
        if (!this.sign_option_revocable.active)
            options |= SIGN_NO_REVOKE;

        // Signer
        Key? signer = ComboKeys.get_active (this.signer_select);
        assert (signer == null || (SEAHORSE_IS_GPGME_KEY (signer) && signer.usage == SEAHORSE_USAGE_PRIVATE_KEY));

        Object to_sign = g_object_get_data (G_OBJECT (swidget), "to-sign");
        GPG.Error err;
        if (to_sign is Uid)
            err = KeyOperation.sign_uid((Uid) to_sign, SEAHORSE_GPGME_KEY (signer), check, options);
        else if (to_sign is Key)
            err = KeyOperation.sign ((Key) to_sign, SEAHORSE_GPGME_KEY (signer), check, options);
        else
            assert_not_reached();

        if (!err.is_success()) {
            if (err.error_code() == GPGError.ErrorCode.EALREADY) {
                Gtk.Dialog dlg = new Gtk.MessageDialog(parent, Gtk.DialogFlags.MODAL,
                                                       Gtk.MessageType.INFO, Gtk.ButtonsType.CLOSE,
                                                       _("This key was already signed by\n“%s”"),
                                                       signer.label);
                dlg.run();
                dlg.destroy();
            } else {
                Util.show_error(null, _("Couldn’t sign key"), err.strerror());
            }
        }

        return true;
    }

    private void on_collection_changed(Gcr.Collection collection, GLib.Object object) {
        GtkWidget *widget = GTK_WIDGET (user_data);
        widget.visible = (collection.get_length() > 1);
    }

    // G_MODULE_EXPORT XXX
    private void on_gpgme_sign_choice_toggled(Gtk.ToggleButton toggle) {
        // Figure out choice
        this.sign_display_not.visible = this.sign_choice_not.active;
        this.sign_display_casual.visible = this.sign_choice_casual.active;
        this.sign_display_careful.visible = this.sign_choice_careful.active;
    }

    private static void sign_internal(Object to_sign, Gtk.Window parent) {
        this.to_sign = to_sign;

        // Some initial checks
        Gcr.Collection collection = seahorse_keyset_pgp_signers_new ();

        // If no signing keys then we can't sign
        if (collection.get_length() == 0) {
            // TODO: We should be giving an error message that allows them to generate or import a key
            Seahorse.Util.show_error(null, _("No keys usable for signing"),
                    _("You have no personal PGP keys that can be used to indicate your trust of this key."));
            return;
        }

        Seahorse.Widget swidget = new Seahorse.Widget("sign", parent);

        // ... Except for when calling this, which is messed up
        this.sign_uid_text.set_markup(Markup.printf_escaped("<i>%s</i>", to_sign.label));

        // Uncheck all selections
        this.sign_choice_not.active = false;
        this.sign_choice_casual.active = false;
        this.sign_choice_careful.active = false;

        // Initial choice
        on_gpgme_sign_choice_toggled(null, swidget);

        // Other question's default state
        this.sign_option_local.active = false;
        this.sign_option_revocable.active = true;

        // Signature area
        collection.added.connect(on_collection_changed);
        collection.removed.connect(on_collection_changed);
        on_collection_changed(collection, null, this.signer_frame);

        // Signer box
        ComboKeys.attach(this.signer_select, collection, null);

        // Image
        this.sign_image.set_from_icon_name(SEAHORSE_ICON_SIGN, Gtk.IconSize.DIALOG);

        show();

        bool do_sign = true;
        while (do_sign) {
            switch (run()) {
                case Gtk.ResponseType.HELP:
                    break;
                case Gtk.ResponseType.OK:
                    do_sign = !sign_ok_clicked(parent);
                    break;
                default:
                    do_sign = false;
                    break;
            }
        }

        destroy();
    }

    public static void seahorse_gpgme_sign_prompt (Key to_sign, GtkWindow *parent) {
        sign_internal (to_sign, parent);
    }

    public static void seahorse_gpgme_sign_prompt_uid (Uid to_sign, GtkWindow *parent) {
        sign_internal (to_sign, parent);
    }
}
