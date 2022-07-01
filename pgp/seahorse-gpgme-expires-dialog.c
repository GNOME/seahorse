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

#include "config.h"

#include "seahorse-gpgme-expires-dialog.h"
#include "seahorse-gpgme-key-op.h"
#include "seahorse-gpgme-subkey.h"

#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

#include <string.h>

struct _SeahorseGpgmeExpiresDialog {
    AdwDialog parent_instance;

    SeahorseGpgmeSubkey *subkey;

    GtkWidget *calendar;
    GtkWidget *never_expires_check;
};

enum {
    PROP_0,
    PROP_SUBKEY,
    N_PROPS
};
static GParamSpec *obj_props[N_PROPS] = { NULL, };

G_DEFINE_TYPE (SeahorseGpgmeExpiresDialog, seahorse_gpgme_expires_dialog, ADW_TYPE_DIALOG)

static void
change_date_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseGpgmeExpiresDialog *self = SEAHORSE_GPGME_EXPIRES_DIALOG (widget);
    gpgme_error_t err;
    g_autoptr(GDateTime) expires = NULL;
    GDateTime *old_expires;

    if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (self->never_expires_check))) {
        g_autoptr(GDateTime) now = NULL;

        expires = gtk_calendar_get_date (GTK_CALENDAR (self->calendar));
        now = g_date_time_new_now_local ();

        if (g_date_time_compare (expires, now) <= 0) {
            seahorse_util_show_error (self->calendar, _("Invalid expiry date"),
                                      _("The expiry date must be in the future"));
            return;
        }
    }

    gtk_widget_set_sensitive (self->calendar, FALSE);

    old_expires = seahorse_pgp_subkey_get_expires (SEAHORSE_PGP_SUBKEY (self->subkey));
    if (expires == old_expires && (expires && g_date_time_equal (old_expires, expires)))
        return;

    err = seahorse_gpgme_key_op_set_expires (self->subkey, expires);
    if (!GPG_IS_OK (err))
        seahorse_gpgme_handle_error (err, _("Couldn’t change expiry date"));

    adw_dialog_close (ADW_DIALOG (self));
}

static void
on_gpgme_expire_toggled (GtkWidget *widget,
                         gpointer user_data)
{
    SeahorseGpgmeExpiresDialog *self = SEAHORSE_GPGME_EXPIRES_DIALOG (user_data);

    gtk_widget_set_sensitive (self->calendar,
                              !gtk_check_button_get_active (GTK_CHECK_BUTTON (self->never_expires_check)));
}

static void
seahorse_gpgme_expires_dialog_get_property (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
    SeahorseGpgmeExpiresDialog *self = SEAHORSE_GPGME_EXPIRES_DIALOG (object);

    switch (prop_id) {
    case PROP_SUBKEY:
        g_value_set_object (value, self->subkey);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_expires_dialog_set_property (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
    SeahorseGpgmeExpiresDialog *self = SEAHORSE_GPGME_EXPIRES_DIALOG (object);

    switch (prop_id) {
    case PROP_SUBKEY:
        g_clear_object (&self->subkey);
        self->subkey = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_expires_dialog_finalize (GObject *obj)
{
    SeahorseGpgmeExpiresDialog *self = SEAHORSE_GPGME_EXPIRES_DIALOG (obj);

    g_clear_object (&self->subkey);

    G_OBJECT_CLASS (seahorse_gpgme_expires_dialog_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_expires_dialog_constructed (GObject *obj)
{
    SeahorseGpgmeExpiresDialog *self = SEAHORSE_GPGME_EXPIRES_DIALOG (obj);
    g_autofree char *title = NULL;
    const char *label;
    GDateTime *expires;

    G_OBJECT_CLASS (seahorse_gpgme_expires_dialog_parent_class)->constructed (obj);

    label = seahorse_pgp_subkey_get_description (SEAHORSE_PGP_SUBKEY (self->subkey));
    title = g_strdup_printf (_("Expiry: %s"), label);
    adw_dialog_set_title (ADW_DIALOG (self), title);

    expires = seahorse_pgp_subkey_get_expires (SEAHORSE_PGP_SUBKEY (self->subkey));
    if (expires == NULL) {
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->never_expires_check), TRUE);
        gtk_widget_set_sensitive (self->calendar, FALSE);
    } else {
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->never_expires_check), FALSE);
        gtk_widget_set_sensitive (self->calendar, TRUE);
        gtk_calendar_select_day (GTK_CALENDAR (self->calendar), expires);
    }
}

static void
seahorse_gpgme_expires_dialog_init (SeahorseGpgmeExpiresDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
seahorse_gpgme_expires_dialog_class_init (SeahorseGpgmeExpiresDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_gpgme_expires_dialog_constructed;
    gobject_class->get_property = seahorse_gpgme_expires_dialog_get_property;
    gobject_class->set_property = seahorse_gpgme_expires_dialog_set_property;
    gobject_class->finalize = seahorse_gpgme_expires_dialog_finalize;

    obj_props[PROP_SUBKEY] =
        g_param_spec_object ("subkey", "Subkey",
                             "Subkey",
                             SEAHORSE_GPGME_TYPE_SUBKEY,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPS, obj_props);

    gtk_widget_class_install_action (widget_class, "change-date", NULL, change_date_action);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-gpgme-expires-dialog.ui");
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeExpiresDialog, calendar);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeExpiresDialog, never_expires_check);
    gtk_widget_class_bind_template_callback (widget_class, on_gpgme_expire_toggled);
}

GtkWidget *
seahorse_gpgme_expires_dialog_new (SeahorseGpgmeSubkey *subkey)
{
    g_return_val_if_fail (SEAHORSE_GPGME_IS_SUBKEY (subkey), NULL);

    return g_object_new (SEAHORSE_GPGME_TYPE_EXPIRES_DIALOG,
                         "subkey", subkey,
                         NULL);
}
