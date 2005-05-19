/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _SEAHORSE_DAEMON_H_
#define _SEAHORSE_DAEMON_H_

/* seahorse-sharing.c ------------------------------------------------------- */

void                seahorse_sharing_init();

void                seahorse_sharing_cleanup();

/* seahorse-hkp-server.c ---------------------------------------------------- */

gboolean            seahorse_hkp_server_start();

void                seahorse_hkp_server_stop();

guint               seahorse_hkp_server_get_port();

gboolean            seahorse_hkp_server_is_running();

#endif /* _SEAHORSE_DAEMON_H_ */
