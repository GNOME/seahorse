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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef __SEAHORSE_KEYSERVER_CONTROL_H__
#define __SEAHORSE_KEYSERVER_CONTROL_H__

#include <gtk/gtk.h>

#define SEAHORSE_TYPE_KEYSERVER_CONTROL		(seahorse_keyserver_control_get_type ())
#define SEAHORSE_KEYSERVER_CONTROL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEYSERVER_CONTROL, SeahorseKeyserverControl))
#define SEAHORSE_KEYSERVER_CONTROL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEYSERVER_CONTROL, SeahorseKeyserverControlClass))
#define SEAHORSE_IS_KEYSERVER_CONTROL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEYSERVER_CONTROL))
#define SEAHORSE_IS_KEYSERVER_CONTROL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEYSERVER_CONTROL))
#define SEAHORSE_KEYSERVER_CONTROL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEYSERVER_CONTROL, SeahorseKeyserverControlClass))

typedef struct _SeahorseKeyserverControl SeahorseKeyserverControl;
typedef struct _SeahorseKeyserverControlClass SeahorseKeyserverControlClass;

/**
 * SeahorseKeyServerControl:
 * @parent_instance: The parent #GtkComboBox
 *
 * A control which allows you to select from a set
 * of keyservers.
 *
 * - Also displays shares for keys found via DNS-SD over the network.
 *
 * Properties:
 *   gconf-key: (gchar*) The GConf key to retrieve and set keyservers.
 *   none-option: (gchar*) Text to display for 'no key server'
 */

struct _SeahorseKeyserverControl {
    GtkComboBox parent_instance;

    /* <public> */
    gchar *settings_key;
    gchar *none_option;
};

struct _SeahorseKeyserverControlClass {
	GtkComboBoxClass parent_class;
};

SeahorseKeyserverControl*   seahorse_keyserver_control_new         (const gchar *settings_key,
                                                                    const gchar *none_option);

gchar*                      seahorse_keyserver_control_selected    (SeahorseKeyserverControl *skc);

#endif /* __SEAHORSE_KEYSERVER_CONTROL_H__ */
