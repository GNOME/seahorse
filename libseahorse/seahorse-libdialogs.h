/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Nate Nielsen
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

#ifndef __SEAHORSE_LIBDIALOGS_H__
#define __SEAHORSE_LIBDIALOGS_H__

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"
#include "seahorse-widget.h"

#define SEAHORSE_PASS_BAD    0x00000001
#define SEAHORSE_PASS_NEW    0x01000000

gpgme_error_t
seahorse_passphrase_get (SeahorseContext *sctx, const gchar *passphrase_hint, 
                            const char* passphrase_info, int prev_bad, int fd);

GList*          seahorse_recipients_get (SeahorseContext    *sctx,
                                         SeahorseKeyPair    **signkey);

SeahorseWidget*	seahorse_signatures_new	(SeahorseContext	*sctx);

void            seahorse_signatures_add (SeahorseContext *sctx, 
                                            SeahorseWidget *widget,
                                            const gchar* data, 
                                            gpgme_verify_result_t status);
                            
void            seahorse_signatures_run (SeahorseContext *sctx, 
                                            SeahorseWidget *swidget);

#endif /* __SEAHORSE_LIBDIALOGS_H__ */
