/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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

#ifndef __SEAHORSE_PROGRESS_H__
#define __SEAHORSE_PROGRESS_H__

#include <gtk/gtk.h>

#include "seahorse-operation.h"
#include "seahorse-context.h"

/* -----------------------------------------------------------------------------
 * SEAHORSE APPBAR HOOKS
 */

void          seahorse_progress_appbar_add_operation  (GtkWidget* appbar,
                                                       SeahorseOperation *operation);

void          seahorse_progress_show                  (SeahorseContext *sctx, 
                                                       SeahorseOperation *operation,
                                                       const gchar *title);

#endif /* __SEAHORSE_PROGRESS_H__ */
