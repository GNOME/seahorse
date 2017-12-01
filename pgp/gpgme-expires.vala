/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

namespace Seahorse {
namespace GpgME {

public class Expires : Gtk.Dialog {

    private SubKey subkey;
    private Gtk.Calendar calendar;
    private Gtk.ToggleButton expire;

    // XXX G_MODULE_EXPORT
    private void on_gpgme_expire_ok_clicked (GtkButton *button) {
        GtkWidget *widget;
        gpgme_error_t err;
        time_t expiry = 0;

        widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "expire"));
        if (!widget.active) {
            Posix.tm when;
            memset (&when, 0, sizeof (when));
            widget = GTK_WIDGET (seahorse_widget_get_widget (swidget, "calendar"));
            widget.get_date((guint*)&(when.tm_year), (guint*)&(when.tm_mon), (guint*)&(when.tm_mday));
            when.tm_year -= 1900;
            expiry = mktime (&when);

            if (expiry <= time (NULL)) {
                Seahorse.Util.show_error (widget, _("Invalid expiry date"),
                                          _("The expiry date must be in the future"));
                return;
            }
        }

        widget = seahorse_widget_get_widget (swidget, "all-controls");
        widget.sensitive = false;

        if (expiry != (time_t) this.subkey.expires) {
            err = KeyOperation.set_expires(this.subkey, expiry);
            if (!err.is_success() || err.is_cancelled())
                Util.show_error(null, _("Couldn’t change expiry date"), err.strerror());
        }

        seahorse_widget_destroy (swidget);
    }

    // XXX G_MODULE_EXPORT
    private void on_gpgme_expire_toggled (GtkWidget *widget) {
        Gtk.Widget expire = GTK_WIDGET (seahorse_widget_get_widget (swidget, "expire"));
        Gtk.Widget cal = GTK_WIDGET (seahorse_widget_get_widget (swidget, "calendar"));

        cal.sensitive = !expire.active;
    }

    public void seahorse_gpgme_expires_new(Subkey subkey, GtkWindow *parent) {
        g_return_if_fail (subkey != NULL);

        Seahorse.Widget swidget = seahorse_widget_new_allow_multiple ("expires", parent);

        Gtk.Widget date = GTK_WIDGET (seahorse_widget_get_widget (swidget, "calendar"));
        Gtk.ToggleButton expire = GTK_WIDGET (seahorse_widget_get_widget (swidget, "expire"));
        ulong expires = subkey.expires;
        if (!expires) {
            expire.active = true;
            date.sensitive = false;
        } else {
            expire.active = false;
            date.sensitive = true;
        }

        if (expires) {
            Posix.tm t;
            time_t time = (time_t)expires;
            if (gmtime_r (&time, &t)) {
                date.select_month(t.tm_mon, t.tm_year + 1900);
                date.select_day(t.tm_mday);
            }
        }

        this.title = _("Expiry: %s").printf(subkey.description);
    }
}

}
}
