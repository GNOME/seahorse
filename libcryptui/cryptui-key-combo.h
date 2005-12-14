/*
 * Seahorse
 *
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

#ifndef __CRYPTUI_KEY_COMBO_H__
#define __CRYPTUI_KEY_COMBO_H__

#include <gtk/gtk.h>

#include "cryptui-key-store.h"

#define CRYPTUI_TYPE_KEY_COMBO               (cryptui_key_combo_get_type ())
#define CRYPTUI_KEY_COMBO(obj)               (GTK_CHECK_CAST ((obj), CRYPTUI_TYPE_KEY_COMBO, CryptUIKeyCombo))
#define CRYPTUI_KEY_COMBO_CLASS(klass)       (GTK_CHECK_CLASS_CAST ((klass), CRYPTUI_TYPE_KEY_COMBO, CryptUIKeyComboClass))
#define CRYPTUI_IS_KEY_COMBO(obj)            (GTK_CHECK_TYPE ((obj), CRYPTUI_TYPE_KEY_COMBO))
#define CRYPTUI_IS_KEY_COMBO_CLASS(klass)    (GTK_CHECK_CLASS_TYPE ((klass), CRYPTUI_TYPE_KEY_COMBO))
#define CRYPTUI_KEY_COMBO_GET_CLASS(obj)     (GTK_CHECK_GET_CLASS ((obj), CRYPTUI_TYPE_KEY_COMBO, CryptUIKeyComboClass))

typedef struct _CryptUIKeyCombo CryptUIKeyCombo;
typedef struct _CryptUIKeyComboClass CryptUIKeyComboClass;

struct _CryptUIKeyCombo {
    GtkComboBox parent;
};

struct _CryptUIKeyComboClass {
    GtkComboBoxClass parent_class;
};

CryptUIKeyCombo*  cryptui_key_combo_new             (CryptUIKeyStore *ckstore);

void              cryptui_key_combo_set_key         (CryptUIKeyCombo *ckcombo,
                                                     const gchar *key);

const gchar*      cryptui_key_combo_get_key         (CryptUIKeyCombo *ckcombo);

#endif /* __CRYPTUI_KEY_COMBO_H__ */
