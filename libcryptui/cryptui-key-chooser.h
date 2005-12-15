/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

#ifndef __CRYPTUI_KEY_CHOOSER_H__
#define __CRYPTUI_KEY_CHOOSER_H__

#include <gtk/gtk.h>

#include "cryptui-keyset.h"

#define CRYPTUI_TYPE_KEY_CHOOSER             (cryptui_key_chooser_get_type ())
#define CRYPTUI_KEY_CHOOSER(obj)             (GTK_CHECK_CAST ((obj), CRYPTUI_TYPE_KEY_CHOOSER, CryptUIKeyChooser))
#define CRYPTUI_KEY_CHOOSER_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), CRYPTUI_TYPE_KEY_CHOOSER, CryptUIKeyChooser))
#define CRYPTUI_IS_KEY_CHOOSER(obj)          (GTK_CHECK_TYPE ((obj), CRYPTUI_TYPE_KEY_CHOOSER))
#define CRYPTUI_IS_KEY_CHOOSER_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), CRYPTUI_TYPE_KEY_CHOOSER))
#define CRYPTUI_KEY_CHOOSER_GET_CLASS(obj)   (GTK_CHECK_GET_CLASS ((obj), CRYPTUI_TYPE_KEY_CHOOSER, CryptUIKeyChooser))

typedef struct _CryptUIKeyChooser CryptUIKeyChooser;
typedef struct _CryptUIKeyChooserPriv CryptUIKeyChooserPriv;
typedef struct _CryptUIKeyChooserClass CryptUIKeyChooserClass;

struct _CryptUIKeyChooser {
    GtkVBox               parent;
 
    /*< private >*/
    CryptUIKeyChooserPriv   *priv;
};

struct _CryptUIKeyChooserClass {
    GtkVBoxClass       parent_class;
};


GType               cryptui_key_chooser_get_type            ();

CryptUIKeyChooser*  cryptui_key_chooser_new                 (CryptUIKeyset *ckset);

GList*              cryptui_key_chooser_get_recipients      (CryptUIKeyChooser *chooser);

void                cryptui_key_chooser_set_recipients      (CryptUIKeyChooser *chooser,
                                                             GList *keys);

const gchar*        cryptui_key_chooser_get_signer          (CryptUIKeyChooser *chooser);

void                cryptui_key_chooser_set_signer          (CryptUIKeyChooser *chooser,
                                                             const gchar *key);

#endif /* __CRYPTUI_KEY_CHOOSER_H__ */
