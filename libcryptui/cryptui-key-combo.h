/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Nate Nielsen 
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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef __CRYPTUI_KEY_COMBO_H__
#define __CRYPTUI_KEY_COMBO_H__

#include <gtk/gtk.h>

#include "cryptui-key-store.h"

#define CRYPTUI_TYPE_KEY_COMBO               GTK_TYPE_COMBO_BOX
#define CRYPTUI_KEY_COMBO(obj)               GTK_COMBO_BOX(obj)
#define CRYPTUI_KEY_COMBO_CLASS(klass)       GTK_COMBO_BOX_CLASS(klass)
#define CRYPTUI_IS_KEY_COMBO(obj)            GTK_IS_COMBO_BOX(obj)
#define CRYPTUI_IS_KEY_COMBO_CLASS(klass)    GTK_IS_COMBO_BOX_CLASS(klass)
#define CRYPTUI_KEY_COMBO_GET_CLASS(obj)     GTK_COMBO_BOX_GET_CLASS(obj)

GtkComboBox*      cryptui_key_combo_new             (CryptUIKeyStore *ckstore);

void              cryptui_key_combo_setup           (GtkComboBox *combo,
                                                     CryptUIKeyStore *ckstore);

void              cryptui_key_combo_set_key         (GtkComboBox *combo,
                                                     const gchar *key);

const gchar*      cryptui_key_combo_get_key         (GtkComboBox *ckcombo);

#endif /* __CRYPTUI_KEY_COMBO_H__ */
