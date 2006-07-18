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
 
#ifndef __CRYPTUI_PRIV_H__
#define __CRYPTUI_PRIV_H__

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

/* 
 * Used internally by libcryptui. 
 */

#ifndef LIBCRYPTUI_BUILD
#error "This header shouldn't be included outside of the libcryptui build."
#endif

#include <gconf/gconf.h>

#include "cryptui.h"
#include "cryptui-defines.h"

/* cryptui.c ---------------------------------------------------------------- */

gboolean        _cryptui_gconf_get_boolean          (const char *key);

gchar*          _cryptui_gconf_get_string           (const char *key);

void            _cryptui_gconf_set_string           (const char *key, const char *value);

guint           _cryptui_gconf_notify               (const char *key, 
                                                     GConfClientNotifyFunc notification_callback,
                                                     gpointer callback_data);

void            _cryptui_gconf_notify_lazy          (const char *key, 
                                                     GConfClientNotifyFunc notification_callback,
                                                     gpointer callback_data, gpointer lifetime);
                                                     
void            _cryptui_gconf_unnotify             (guint notification_id);

/* cryptui-keyset.c --------------------------------------------------------- */

const gchar*    _cryptui_keyset_get_internal_keyid  (CryptUIKeyset *ckset,
                                                     const gchar *keyid);

#endif /* __CRYPTUI_PRIV_H__ */
