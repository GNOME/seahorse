/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __SEAHORSE_PKCS11_GENERATE_H__
#define __SEAHORSE_PKCS11_GENERATE_H__

#include <gtk/gtk.h>

#define      SEAHORSE_TYPE_PKCS11_GENERATE        (seahorse_pkcs11_generate_get_type ())

GType        seahorse_pkcs11_generate_get_type    (void) G_GNUC_CONST;

void         seahorse_pkcs11_generate_prompt      (GtkWindow *parent);

void         seahorse_pkcs11_generate_register    (void);

#endif /* __SEAHORSE_PKCS11_GENERATE_H__ */
