/*
 * Seahorse
 *
 * Copyright (C) 2006 Adam Schreiber
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_KEYSERVER_SYNC_H__
#define __SEAHORSE_KEYSERVER_SYNC_H__

void        seahorse_keyserver_sync             (GList *keys);


GtkWindow*  seahorse_keyserver_sync_show        (GList *keys,
                                                 GtkWindow *parent);

#endif
