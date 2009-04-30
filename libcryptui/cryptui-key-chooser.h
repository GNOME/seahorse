/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Stefan Walter
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

#ifndef __CRYPTUI_KEY_CHOOSER_H__
#define __CRYPTUI_KEY_CHOOSER_H__

#include <gtk/gtk.h>

#include "cryptui-keyset.h"

typedef enum _CryptUIKeyChooserMode {
    CRYPTUI_KEY_CHOOSER_RECIPIENTS =    0x0001,
    CRYPTUI_KEY_CHOOSER_SIGNER =        0x0002,
    
    CRYPTUI_KEY_CHOOSER_MUSTSIGN =      0x0010
} CryptUIKeyChooserMode;

#define CRYPTUI_TYPE_KEY_CHOOSER             (cryptui_key_chooser_get_type ())
#define CRYPTUI_KEY_CHOOSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CRYPTUI_TYPE_KEY_CHOOSER, CryptUIKeyChooser))
#define CRYPTUI_KEY_CHOOSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CRYPTUI_TYPE_KEY_CHOOSER, CryptUIKeyChooser))
#define CRYPTUI_IS_KEY_CHOOSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CRYPTUI_TYPE_KEY_CHOOSER))
#define CRYPTUI_IS_KEY_CHOOSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CRYPTUI_TYPE_KEY_CHOOSER))
#define CRYPTUI_KEY_CHOOSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CRYPTUI_TYPE_KEY_CHOOSER, CryptUIKeyChooser))

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
    
    /* signals --------------------------------------------------------- */
    
    /* The key selection changed  */
    void (*changed)   (CryptUIKeyChooser *chooser);
};


GType               cryptui_key_chooser_get_type            ();

CryptUIKeyChooser*  cryptui_key_chooser_new                 (CryptUIKeyset *ckset, 
                                                             CryptUIKeyChooserMode mode);

gboolean            cryptui_key_chooser_get_enforce_prefs   (CryptUIKeyChooser *chooser);

void                cryptui_key_chooser_set_enforce_prefs   (CryptUIKeyChooser *chooser,
                                                             gboolean enforce_prefs);

gboolean            cryptui_key_chooser_have_recipients     (CryptUIKeyChooser *chooser);

GList*              cryptui_key_chooser_get_recipients      (CryptUIKeyChooser *chooser);

void                cryptui_key_chooser_set_recipients      (CryptUIKeyChooser *chooser,
                                                             GList *keys);

const gchar*        cryptui_key_chooser_get_signer          (CryptUIKeyChooser *chooser);

void                cryptui_key_chooser_set_signer          (CryptUIKeyChooser *chooser,
                                                             const gchar *key);

CryptUIKeyChooserMode 
                    cryptui_key_chooser_get_mode            (CryptUIKeyChooser *chooser);
#endif /* __CRYPTUI_KEY_CHOOSER_H__ */
