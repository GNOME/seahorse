/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
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
