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

#include "seahorse-progress.h"

/* Called to keep the progress bar cycle when we're in pulse mode */
static gboolean
pulse_timer (GtkWidget *appbar)
{
    g_return_val_if_fail (GNOME_IS_APPBAR (appbar), FALSE);
    gtk_progress_bar_pulse (gnome_appbar_get_progress (GNOME_APPBAR (appbar)));
    return TRUE;
}

/* This gets called whenever an operation updates it's progress status thingy.
 * We update the appbar as appropriate. If operation != NULL then we only 
 * display the latest operation in our special list  */
static void
operation_progress (SeahorseOperation *operation, const gchar *message, 
                    gdouble fract, GtkWidget *appbar)
{
    GtkProgressBar *progress;
    guint stag;
  
    g_return_if_fail (GNOME_IS_APPBAR (appbar));
    
    if (message != NULL)
        gnome_appbar_set_status (GNOME_APPBAR (appbar), message);

    progress = gnome_appbar_get_progress (GNOME_APPBAR (appbar));
        
    if (fract >= 0.0) {
        gtk_progress_bar_set_fraction (progress, fract);
        
        /* This will stop the pulse timer */
        g_object_set_data (G_OBJECT (appbar), "pulse-timer", NULL);

    } else { 

        gtk_progress_bar_set_pulse_step (progress, 0.05);
        gtk_progress_bar_pulse (progress);
        
        stag = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (appbar), "pulse-timer"));
        if (stag == 0) {
            /* Start a pulse timer */
            stag = g_timeout_add (100, (GSourceFunc)pulse_timer, appbar); 
            g_object_set_data_full (G_OBJECT (appbar), "pulse-timer", 
                                    GINT_TO_POINTER (stag), (GDestroyNotify)g_source_remove);
        }
    }
}

static void
operation_done (SeahorseOperation *operation, GtkWidget *appbar)
{
    /* Done, so cleanup the display */
    operation_progress (operation, NULL, 0.0, appbar);
}

void 
seahorse_progress_appbar_add_operation (GtkWidget* appbar, SeahorseOperation *operation)
{
    SeahorseMultiOperation *mop;
    
    g_return_if_fail (GNOME_IS_APPBAR (appbar));
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    
    if (seahorse_operation_is_done (operation))
        return;
        
    mop = SEAHORSE_MULTI_OPERATION (g_object_get_data (G_OBJECT (appbar), "operations"));
    if (mop == NULL) {
        mop = seahorse_multi_operation_new ();
        g_signal_connect (mop, "done", G_CALLBACK (operation_done), appbar);
        g_signal_connect (mop, "progress", G_CALLBACK (operation_progress), appbar);
        g_object_set_data_full (G_OBJECT (appbar), "operations", mop, 
                                (GDestroyNotify)g_object_unref);
    }

    seahorse_multi_operation_add (mop, operation);    
}
