/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

/**
 * Function for displaying the preferences dialog.
 */
 
#ifndef __SEAHORSE_PREFS_H__
#define __SEAHORSE_PREFS_H__

#include "seahorse-widget.h"

gboolean            seahorse_prefs_available    (void);

void                seahorse_prefs_show         (GtkWindow *parent,
                                                 const gchar *tabid);

SeahorseWidget *    seahorse_prefs_new          (GtkWindow *parent);

void                seahorse_prefs_add_tab      (SeahorseWidget *swidget,
                                                 GtkWidget *label,
                                                 GtkWidget *tab);
                                                 
void                seahorse_prefs_select_tab   (SeahorseWidget *swidget,
                                                 GtkWidget *tab);

void                seahorse_prefs_select_tabid (SeahorseWidget *swidget,
                                                 const gchar *tab);

void                seahorse_prefs_remove_tab   (SeahorseWidget *swidget,
                                                 GtkWidget *tab);

#endif /* __SEAHORSE_PREFERENCES_H__ */       
