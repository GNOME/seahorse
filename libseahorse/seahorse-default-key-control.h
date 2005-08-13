/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Nate Nielsen
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

/** 
 * SeahorseDefaultKeyControl: (misnamed) Shows a list of keys in a dropdown for 
 * selection.
 * 
 * - Derived from GtkOptionMenu
 * - Gets its list of keys from a SeahorseKeyset.
 * 
 * Properties:
 *   keyset: (SeahorseKeyset) The keyset being used for listing keys.
 *   none-option: (gchar*) The option text used for selection 'no key'
 */
 
#ifndef __SEAHORSE_DEFAULT_KEY_CONTROL_H__
#define __SEAHORSE_DEFAULT_KEY_CONTROL_H__

#include <gtk/gtk.h>
#include "seahorse-keyset.h"

#define SEAHORSE_TYPE_DEFAULT_KEY_CONTROL		(seahorse_default_key_control_get_type ())
#define SEAHORSE_DEFAULT_KEY_CONTROL(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_DEFAULT_KEY_CONTROL, SeahorseDefaultKeyControl))
#define SEAHORSE_DEFAULT_KEY_CONTROL_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_DEFAULT_KEY_CONTROL, SeahorseDefaultKeyControlClass))
#define SEAHORSE_IS_DEFAULT_KEY_CONTROL(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_DEFAULT_KEY_CONTROL))
#define SEAHORSE_IS_DEFAULT_KEY_CONTROL_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_DEFAULT_KEY_CONTROL))
#define SEAHORSE_DEFAULT_KEY_CONTROL_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_DEFAULT_KEY_CONTROL, SeahorseDefaultKeyControlClass))

typedef struct _SeahorseDefaultKeyControl SeahorseDefaultKeyControl;
typedef struct _SeahorseDefaultKeyControlClass SeahorseDefaultKeyControlClass;

struct _SeahorseDefaultKeyControl {
    GtkOptionMenu   parent;
    
    /*<public>*/
    SeahorseKeyset  *skset;
};

struct _SeahorseDefaultKeyControlClass {
	GtkCheckButtonClass	parent_class;
};

SeahorseDefaultKeyControl*  seahorse_default_key_control_new       (SeahorseKeyset    *skset,
                                                                    const gchar       *none_option);

void                        seahorse_default_key_control_select_id (SeahorseDefaultKeyControl *sdkc,
                                                                    const gchar *id);

void                        seahorse_default_key_control_select    (SeahorseDefaultKeyControl *sdkc,
                                                                    SeahorseKey *skey);

SeahorseKey*                seahorse_default_key_control_active    (SeahorseDefaultKeyControl *sdkc);

const gchar*                seahorse_default_key_control_active_id (SeahorseDefaultKeyControl *sdkc);

#endif /* __SEAHORSE_DEFAULT_KEY_CONTROL_H__ */
