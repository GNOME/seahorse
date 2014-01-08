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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef SEAHORSESSHDIALOGS_H_
#define SEAHORSESSHDIALOGS_H_

#include <gtk/gtk.h>

#include "seahorse-ssh-key.h"
#include "seahorse-ssh-source.h"

void        seahorse_ssh_upload_prompt         (GList *keys,
                                                GtkWindow *parent);

GtkWindow * seahorse_ssh_key_properties_show   (SeahorseSSHKey *skey,
                                                GtkWindow *parent);

void        seahorse_ssh_generate_show         (SeahorseSSHSource *sksrc,
                                                GtkWindow *parent);

void        seahorse_ssh_generate_register     (void);

#endif /*SEAHORSESSHDIALOGS_H_*/
