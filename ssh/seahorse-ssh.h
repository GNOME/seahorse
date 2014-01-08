/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_SSH_H__
#define __SEAHORSE_SSH_H__

#include <glib.h>
#include <glib-object.h>

#include <gcr/gcr.h>

#define SEAHORSE_SSH_NAME "openssh"
#define SEAHORSE_SSH_TYPE_STR SEAHORSE_SSH_NAME
#define SEAHORSE_SSH (g_quark_from_static_string (SEAHORSE_SSH_NAME))

#ifdef WITH_SSH

void       seahorse_ssh_backend_initialize    (void);

#endif /* WITH_SSH */

#endif
