/*
 * Seahorse
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_PKCS11_GENERATE_H__
#define __SEAHORSE_PKCS11_GENERATE_H__

#include <gtk/gtk.h>

#define      SEAHORSE_TYPE_PKCS11_GENERATE        (seahorse_pkcs11_generate_get_type ())

GType        seahorse_pkcs11_generate_get_type    (void) G_GNUC_CONST;

void         seahorse_pkcs11_generate_prompt      (GtkWindow *parent);

void         seahorse_pkcs11_generate_register    (void);

#endif /* __SEAHORSE_PKCS11_GENERATE_H__ */
