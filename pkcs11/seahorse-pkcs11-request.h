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
 *
 * Author: Stef Walter <stef@collabora.co.uk>
 */

#ifndef __SEAHORSE_PKCS11_REQUEST_H__
#define __SEAHORSE_PKCS11_REQUEST_H__

#include <gtk/gtk.h>

#include <gck/gck.h>

void         seahorse_pkcs11_request_prompt      (GtkWindow *parent,
                                                  GckObject *private_key);

#endif /* __SEAHORSE_PKCS11_REQUEST_H__ */
