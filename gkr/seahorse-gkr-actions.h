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
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __SEAHORSE_GKR_ACTIONS_H__
#define __SEAHORSE_GKR_ACTIONS_H__

#include "seahorse-gkr-backend.h"
#include "seahorse-gkr-keyring.h"

#include <gtk/gtk.h>

GtkActionGroup *     seahorse_gkr_backend_actions_instance      (SeahorseGkrBackend *backend);

GtkActionGroup *     seahorse_gkr_keyring_actions_new           (SeahorseGkrKeyring *keyring);

#endif /* __SEAHORSE_GKR_ACTIONS_H__ */
