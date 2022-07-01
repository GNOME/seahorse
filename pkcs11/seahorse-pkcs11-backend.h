/*
 * Seahorse
 *
 * Copyright (C) 2008 Stefan Walter
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

#ifndef SEAHORSE_PKCS11_BACKEND_H_
#define SEAHORSE_PKCS11_BACKEND_H_

#include <gcr/gcr.h>

#define SEAHORSE_PKCS11_STR                    "pkcs11"
#define SEAHORSE_PKCS11                         (g_quark_from_static_string (SEAHORSE_PKCS11_STR))

#define SEAHORSE_TYPE_PKCS11_BACKEND            (seahorse_pkcs11_backend_get_type ())
G_DECLARE_FINAL_TYPE (SeahorsePkcs11Backend, seahorse_pkcs11_backend, SEAHORSE, PKCS11_BACKEND, GObject)

SeahorsePkcs11Backend *  seahorse_pkcs11_backend_get           (void);

void  seahorse_pkcs11_backend_initialize (void);

#endif /* SEAHORSE_PKCS11_BACKEND_H_ */
