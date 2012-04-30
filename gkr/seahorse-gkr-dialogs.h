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

#ifndef __SEAHORSE_GKR_DIALOGS__
#define __SEAHORSE_GKR_DIALOGS__

#include <gtk/gtk.h>

#include "seahorse-gkr-item.h"
#include "seahorse-gkr-keyring.h"
#include "seahorse-widget.h"

void            seahorse_gkr_add_keyring_register     (void);

void            seahorse_gkr_add_item_show            (GtkWindow *parent);

void            seahorse_gkr_add_keyring_show         (GtkWindow *parent);

void            seahorse_gkr_item_properties_show     (SeahorseGkrItem *git, GtkWindow *parent);

void            seahorse_gkr_keyring_properties_show  (SeahorseGkrKeyring *gkr, GtkWindow *parent);

GCancellable *  seahorse_gkr_dialog_begin_request     (SeahorseWidget *swidget);

void            seahorse_gkr_dialog_complete_request  (SeahorseWidget *swidget, gboolean cancel);

#endif /* __SEAHORSE_GKR_DIALOGS__ */
