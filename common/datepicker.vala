/*
 * Seahorse
 *
 * Copyright (C) 2021 Niels De Graef
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

/**
 * Shows a configurable date. A user can configure it by either manually
 * changing a {@link Gtk.Entry} or by opening up a popover with a
 * {@link Gtk.Calendar} and finding the right date there.
 */
public class Seahorse.DatePicker : Gtk.Box {

    private Gtk.Entry date_entry;
    private Gtk.Popover calendar_popover;
    private Gtk.Calendar calendar;

    private GLib.DateTime _datetime = new GLib.DateTime.now();
    public GLib.DateTime datetime {
        get { return this._datetime; }
        set {
          if (this._datetime.equal(value))
            return;

          this._datetime = value;
          this.date_entry.text = value.format("%F");
          this.calendar.select_day(value);
       }
    }

    construct {
        this.orientation = Gtk.Orientation.HORIZONTAL;
        add_css_class("linked");

        // The entry for manual editing
        this.date_entry = new Gtk.Entry();
        this.date_entry.max_length = 10;
        this.date_entry.max_width_chars = 10;
        this.date_entry.tooltip_text = _("Enter the date directly");
        this.date_entry.activate.connect(on_date_entry_activated);
        append(this.date_entry);

        // The button with a popover
        var calendar_button = new Gtk.MenuButton();
        calendar_button.tooltip_text = _("Select the date from a calendar");
        append(calendar_button);

        // The popover that contains the calendar
        this.calendar_popover = new Gtk.Popover();
        calendar_button.popover = this.calendar_popover;

        // The calendar
        this.calendar = new Gtk.Calendar();
        this.calendar.show_day_names = true;
        this.calendar.show_heading = true;
        this.calendar.day_selected.connect(on_calendar_day_selected);
        this.calendar_popover.set_child(this.calendar);

        // Pop down the calendar when a date is selected with double click
        var gesture = new Gtk.GestureClick();
        gesture.button = Gdk.BUTTON_PRIMARY;
        gesture.pressed.connect(on_calendar_primary_click);
        this.calendar.add_controller(gesture);
    }

    private void on_date_entry_activated(Gtk.Entry date_entry) {
        int y, m, d;

        // FIXME show something on an invalid date
        if (date_entry.get_text().scanf("%d-%d-%d", out y, out m, out d) != 3)
            return;

        var parsed_date = new DateTime.utc(y, m, d, 0, 0, 0);
        // FIXME warn on invalid date, or date before today
        if (parsed_date == null)
            return;

        this.datetime = parsed_date;
    }

    private void on_calendar_day_selected(Gtk.Calendar calendar) {
        // Don't use calendar.get_date() directly, as it will return a time at
        // midnight in the local time zone, which -when converted to UTC can go
        // back a day or the next.
        this.datetime = new DateTime.utc(calendar.year, calendar.month, calendar.day,
                                         0, 0, 0);
    }

    private void on_calendar_primary_click(Gtk.GestureClick gesture, int n_press, double x, double y) {
        if (n_press != 2)
            return;

        var selected_date = this.calendar.get_date();
        if (selected_date != null)
            this.calendar_popover.popdown();
    }
}
