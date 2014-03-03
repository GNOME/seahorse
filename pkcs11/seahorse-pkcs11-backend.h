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

#include "seahorse-pkcs11.h"

#include <gcr/gcr.h>

#define SEAHORSE_PKCS11_STR                    "pkcs11"
#define SEAHORSE_PKCS11                         (g_quark_from_static_string (SEAHORSE_PKCS11_STR))

#define SEAHORSE_TYPE_PKCS11_BACKEND            (seahorse_pkcs11_backend_get_type ())
#define SEAHORSE_PKCS11_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_PKCS11_BACKEND, SeahorsePkcs11Backend))
#define SEAHORSE_PKCS11_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_PKCS11_BACKEND, SeahorsePkcs11BackendClass))
#define SEAHORSE_IS_PKCS11_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_PKCS11_BACKEND))
#define SEAHORSE_IS_PKCS11_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_PKCS11_BACKEND))
#define SEAHORSE_PKCS11_BACKEND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_PKCS11_BACKEND, SeahorsePkcs11BackendClass))

typedef struct _SeahorsePkcs11Backend SeahorsePkcs11Backend;
typedef struct _SeahorsePkcs11BackendClass SeahorsePkcs11BackendClass;

GType                    seahorse_pkcs11_backend_get_type      (void) G_GNUC_CONST;

SeahorsePkcs11Backend *  seahorse_pkcs11_backend_get           (void);

GcrCollection *          seahorse_pkcs11_backend_get_writable_tokens (SeahorsePkcs11Backend *self,
                                                                      gulong with_mechanism);

#endif /* SEAHORSE_PKCS11_BACKEND_H_ */
