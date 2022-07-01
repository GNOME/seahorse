/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2005 Jim Pharis
 * Copyright (C) 2005-2006 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2019 Niels De Graef
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

#include "seahorse-gpgme-keyring-panel.h"
#include "seahorse-gpgme.h"

#include "seahorse-common.h"

#include <glib.h>
#include <glib/gi18n.h>

enum {
    PROP_KEYRING = 1,
    N_PROPS
};
static GParamSpec *properties[N_PROPS] = { NULL, };

struct _SeahorseGpgmeKeyringPanel {
    SeahorsePanel parent_instance;

    SeahorseGpgmeKeyring *keyring;

    GtkWidget *homedir_row;
    GtkWidget *version_row;
};

G_DEFINE_TYPE (SeahorseGpgmeKeyringPanel, seahorse_gpgme_keyring_panel, SEAHORSE_TYPE_PANEL)

static void
seahorse_gpgme_keyring_panel_get_property (GObject      *object,
                                           unsigned int  prop_id,
                                           GValue       *value,
                                           GParamSpec   *pspec)
{
    SeahorseGpgmeKeyringPanel *self = SEAHORSE_GPGME_KEYRING_PANEL (object);

    switch (prop_id) {
    case PROP_KEYRING:
        g_value_set_object (value, self->keyring);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_keyring_panel_set_property (GObject      *object,
                                     unsigned int  prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    SeahorseGpgmeKeyringPanel *self = SEAHORSE_GPGME_KEYRING_PANEL (object);

    switch (prop_id) {
    case PROP_KEYRING:
        g_set_object (&self->keyring, g_value_get_object (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
seahorse_gpgme_keyring_panel_finalize (GObject *obj)
{
    SeahorseGpgmeKeyringPanel *self = SEAHORSE_GPGME_KEYRING_PANEL (obj);

    g_clear_object (&self->keyring);

    G_OBJECT_CLASS (seahorse_gpgme_keyring_panel_parent_class)->finalize (obj);
}

static void
seahorse_gpgme_keyring_panel_dispose (GObject *obj)
{
    SeahorseGpgmeKeyringPanel *self = SEAHORSE_GPGME_KEYRING_PANEL (obj);
    GtkWidget *child = NULL;

    while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))) != NULL)
        gtk_widget_unparent (child);

    G_OBJECT_CLASS (seahorse_gpgme_keyring_panel_parent_class)->dispose (obj);
}

static void
seahorse_gpgme_keyring_panel_constructed (GObject *obj)
{
    SeahorseGpgmeKeyringPanel *self = SEAHORSE_GPGME_KEYRING_PANEL (obj);
	gpgme_error_t gerr;
	gpgme_engine_info_t engine;
    const char *homedir;

    G_OBJECT_CLASS (seahorse_gpgme_keyring_panel_parent_class)->constructed (obj);

    gerr = gpgme_get_engine_info (&engine);
	g_return_if_fail (GPG_IS_OK (gerr));

    homedir = engine->home_dir? engine->home_dir : gpgme_get_dirinfo ("homedir");
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self->homedir_row), homedir);

    if (engine->version != NULL)
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->version_row), engine->version);
    else
        gtk_widget_set_visible (self->version_row, FALSE);
}

static void
seahorse_gpgme_keyring_panel_init (SeahorseGpgmeKeyringPanel *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
seahorse_gpgme_keyring_panel_class_init (SeahorseGpgmeKeyringPanelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = seahorse_gpgme_keyring_panel_constructed;
    gobject_class->get_property = seahorse_gpgme_keyring_panel_get_property;
    gobject_class->set_property = seahorse_gpgme_keyring_panel_set_property;
    gobject_class->dispose = seahorse_gpgme_keyring_panel_dispose;
    gobject_class->finalize = seahorse_gpgme_keyring_panel_finalize;

    properties[PROP_KEYRING] =
        g_param_spec_object ("keyring", NULL, NULL,
                             SEAHORSE_TYPE_GPGME_KEYRING,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (gobject_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Seahorse/seahorse-gpgme-keyring-panel.ui");
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorseGpgmeKeyringPanel,
                                          homedir_row);
    gtk_widget_class_bind_template_child (widget_class,
                                          SeahorseGpgmeKeyringPanel,
                                          version_row);
}

GtkWidget *
seahorse_gpgme_keyring_panel_new (SeahorseGpgmeKeyring *keyring)
{
    g_autoptr(SeahorseGpgmeKeyringPanel) self = NULL;

    g_return_val_if_fail (SEAHORSE_IS_GPGME_KEYRING (keyring), NULL);

    self = g_object_new (SEAHORSE_GPGME_TYPE_KEYRING_PANEL,
                         "keyring", keyring,
                         NULL);

    return GTK_WIDGET (g_steal_pointer (&self));
}
