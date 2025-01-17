/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "seahorse-gpgme-add-uid.h"

#include "seahorse-gpgme-key-op.h"
#include "libseahorse/seahorse-util.h"

#include <glib/gi18n.h>

/**
 * SECTION:seahorse-gpgme-add-uid
 * @short_description: A dialog that allows a user to add a new UID to a key
 **/

struct _SeahorseGpgmeAddUid {
    GtkApplicationWindow parent_instance;

    SeahorseGpgmeKey *key;

    GtkWidget *name_row;
    GtkWidget *email_row;
    GtkWidget *comment_row;
};

enum {
    PROP_0,
    PROP_KEY,
};

G_DEFINE_TYPE (SeahorseGpgmeAddUid, seahorse_gpgme_add_uid, GTK_TYPE_APPLICATION_WINDOW)

static void
on_gpgme_key_op_uid_added (GObject *source, GAsyncResult *result, gpointer user_data)
{
    SeahorseGpgmeAddUid *dialog = SEAHORSE_GPGME_ADD_UID (user_data);
    SeahorseGpgmeKey *key = SEAHORSE_GPGME_KEY (source);
    g_autoptr(GError) error = NULL;

    if (!seahorse_gpgme_key_op_add_uid_finish (key, result, &error)) {
        GtkWindow *window = gtk_window_get_transient_for (GTK_WINDOW (dialog));
        seahorse_util_show_error (GTK_WIDGET (window),
                                  _("Couldn’t add user ID"),
                                  error->message);
        return;
    }

    seahorse_gpgme_key_refresh (key);
    gtk_window_close (GTK_WINDOW (dialog));
}

static void
add_uid_action (GtkWidget *widget, const char *action_name, GVariant *param)
{
    SeahorseGpgmeAddUid *dialog = SEAHORSE_GPGME_ADD_UID (widget);
    const char *name, *email, *comment;

    name = gtk_editable_get_text (GTK_EDITABLE (dialog->name_row));
    email = gtk_editable_get_text (GTK_EDITABLE (dialog->email_row));
    comment = gtk_editable_get_text (GTK_EDITABLE (dialog->comment_row));

    seahorse_gpgme_key_op_add_uid_async (dialog->key,
                                         name, email, comment,
                                         NULL,
                                         on_gpgme_key_op_uid_added,
                                         dialog);
}

static gboolean
check_name_input (SeahorseGpgmeAddUid *self)
{
    const char *name;

    name = gtk_editable_get_text (GTK_EDITABLE (self->name_row));
    return strlen (name) >= 5;
}

static gboolean
check_email_input (SeahorseGpgmeAddUid *self)
{
    const char *email;

    email = gtk_editable_get_text (GTK_EDITABLE (self->email_row));
    return strlen (email) == 0 || g_pattern_match_simple ("?*@?*", email);
}

static void
check_ok (SeahorseGpgmeAddUid *self)
{
    gboolean enabled;

    enabled = check_name_input (self) && check_email_input (self);
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "add-uid", enabled);
}

static void
on_name_row_changed (GtkEditable *editable, gpointer user_data)
{
    SeahorseGpgmeAddUid *self = SEAHORSE_GPGME_ADD_UID (user_data);
    check_ok (self);
}

static void
on_email_row_changed (GtkEditable *editable, gpointer user_data)
{
    SeahorseGpgmeAddUid *self = SEAHORSE_GPGME_ADD_UID (user_data);
    check_ok (self);
}

static void
seahorse_gpgme_add_uid_get_property (GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    SeahorseGpgmeAddUid *self = SEAHORSE_GPGME_ADD_UID (object);

    switch (prop_id) {
    case PROP_KEY:
        g_value_set_object (value, self->key);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_add_uid_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    SeahorseGpgmeAddUid *self = SEAHORSE_GPGME_ADD_UID (object);

    switch (prop_id) {
    case PROP_KEY:
        g_clear_object (&self->key);
        self->key = g_value_dup_object (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_add_uid_finalize (GObject *obj)
{
    SeahorseGpgmeAddUid *self = SEAHORSE_GPGME_ADD_UID (obj);

    g_clear_object (&self->key);

    G_OBJECT_CLASS (seahorse_gpgme_add_uid_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_add_uid_constructed (GObject *obj)
{
    SeahorseGpgmeAddUid *self = SEAHORSE_GPGME_ADD_UID (obj);
    const gchar *user;

    G_OBJECT_CLASS (seahorse_gpgme_add_uid_parent_class)->constructed (obj);

    user = seahorse_item_get_title (SEAHORSE_ITEM (self->key));
    gtk_window_set_title (GTK_WINDOW (self),
                          g_strdup_printf (_("Add user ID to %s"), user));
}

static void
seahorse_gpgme_add_uid_init (SeahorseGpgmeAddUid *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    check_ok (self);
}

static void
seahorse_gpgme_add_uid_class_init (SeahorseGpgmeAddUidClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_gpgme_add_uid_constructed;
    gobject_class->get_property = seahorse_gpgme_add_uid_get_property;
    gobject_class->set_property = seahorse_gpgme_add_uid_set_property;
    gobject_class->finalize = seahorse_gpgme_add_uid_finalize;

    g_object_class_install_property (gobject_class, PROP_KEY,
        g_param_spec_object ("key", "GPGME key",
                             "The GPGME key which we're adding a new UID to",
                             SEAHORSE_GPGME_TYPE_KEY,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    gtk_widget_class_install_action (widget_class, "add-uid", NULL, add_uid_action);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Seahorse/seahorse-gpgme-add-uid.ui");

    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddUid, name_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddUid, email_row);
    gtk_widget_class_bind_template_child (widget_class, SeahorseGpgmeAddUid, comment_row);

    gtk_widget_class_bind_template_callback (widget_class, on_name_row_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_email_row_changed);
}

/**
 * seahorse_add_uid_new:
 * @skey: #SeahorseKey
 *
 * Creates a new #SeahorseGpgmeAddUid dialog for adding a user ID to @skey.
 */
SeahorseGpgmeAddUid *
seahorse_gpgme_add_uid_new (SeahorseGpgmeKey *pkey, GtkWindow *parent)
{
    g_autoptr(SeahorseGpgmeAddUid) self = NULL;

    g_return_val_if_fail (SEAHORSE_GPGME_IS_KEY (pkey), NULL);

    self = g_object_new (SEAHORSE_GPGME_TYPE_ADD_UID,
                         "key", pkey,
                         "transient-for", parent,
                         NULL);

    return g_steal_pointer (&self);
}

void
seahorse_gpgme_add_uid_run_dialog (SeahorseGpgmeKey *pkey, GtkWindow *parent)
{
    g_autoptr(SeahorseGpgmeAddUid) dialog = NULL;

    g_return_if_fail (SEAHORSE_GPGME_IS_KEY (pkey));

    dialog = seahorse_gpgme_add_uid_new (pkey, parent);
    gtk_window_present (GTK_WINDOW (g_steal_pointer (&dialog)));
}
