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

#include "seahorse-progress.h"
#include "seahorse-widget.h"

/* -----------------------------------------------------------------------------
 *  GENERIC PROGRESS BAR HANDLING 
 */
 
static void     disconnect_progress     (SeahorseWidget *widget, SeahorseOperation *op);
 
/* Called to keep the progress bar cycle when we're in pulse mode */
static gboolean
pulse_timer (GtkProgressBar *progress)
{
    g_assert (GTK_IS_PROGRESS_BAR (progress));
    
    if (gtk_progress_bar_get_pulse_step (progress) != 0) {
        gtk_progress_bar_pulse (progress);
         return TRUE;
    }
    
    return FALSE;
}

static void
start_pulse (GtkProgressBar *progress)
{
    guint stag;
   
    gtk_progress_bar_set_pulse_step (progress, 0.05);
    gtk_progress_bar_pulse (progress);
        
    stag = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (progress), "pulse-timer"));    
    if (stag == 0) {
        /* Start a pulse timer */
        stag = g_timeout_add (100, (GSourceFunc)pulse_timer, progress); 
        g_object_set_data_full (G_OBJECT (progress), "pulse-timer", 
                                GINT_TO_POINTER (stag), (GDestroyNotify)g_source_remove);
    }   
}

static void
stop_pulse (GtkProgressBar *progress)
{
    gtk_progress_bar_set_pulse_step (progress, 0.0);
    
    /* This causes the destroy callback to be called */
    g_object_set_data (G_OBJECT (progress), "pulse-timer", NULL);
}

/* This gets called whenever an operation updates it's progress status thingy.
 * We update the appbar as appropriate. If operation != NULL then we only 
 * display the latest operation in our special list  */
static void
operation_progress (SeahorseOperation *operation, const gchar *message, 
                    gdouble fract, SeahorseWidget *swidget)
{
    GtkProgressBar *progress;
    GtkStatusbar *status;
    guint id; 
    
    progress = GTK_PROGRESS_BAR (seahorse_widget_get_widget (swidget, "progress"));
    status = GTK_STATUSBAR (seahorse_widget_get_widget (swidget, "status"));
    
    if (!seahorse_operation_is_running (operation))
        fract = 0.0;
    
    if (message != NULL && status) {
        g_return_if_fail (GTK_IS_STATUSBAR (status));
        id = gtk_statusbar_get_context_id (status, "operation-progress");
        gtk_statusbar_pop (status, id);
        if (message[0])
            gtk_statusbar_push (status, id, message);
    }

    if(progress) {
        g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));
        if (fract >= 0.0) {
            stop_pulse (progress);
            gtk_progress_bar_set_fraction (progress, fract);        
        } else { 
            start_pulse (progress);
        }
    }
}

static void
operation_done (SeahorseOperation *operation, SeahorseWidget *swidget)
{
    GError *err = NULL;

    if (!seahorse_operation_is_successful (operation)) {
        seahorse_operation_copy_error (operation, &err);
        if (err) {
            operation_progress (operation, err->message, 0.0, swidget);
            g_error_free (err);
            return;
        }
    }
    
    operation_progress (operation, "", 0.0, swidget);
}

static void
disconnect_progress (SeahorseWidget *widget, SeahorseOperation *op)
{
    g_signal_handlers_disconnect_by_func (op, operation_progress, widget);
    g_signal_handlers_disconnect_by_func (op, operation_done, widget);
    g_signal_handlers_disconnect_by_func (widget, disconnect_progress, op);
}

void 
seahorse_progress_status_set_operation (SeahorseWidget *swidget, 
                                        SeahorseOperation *operation)
{
    SeahorseOperation *prev;
    
    /* 
     * Note that this is not one off, the operation is monitored until it is 
     * replaced, so if the operation starts up again the progress will be 
     * displayed 
     */
    
    g_return_if_fail (SEAHORSE_IS_WIDGET (swidget));
    g_return_if_fail (SEAHORSE_IS_OPERATION (operation));
    
    prev = SEAHORSE_OPERATION (g_object_get_data (G_OBJECT (swidget), "operation"));
    if (prev) {
	    
        /* If it's the same operation, just ignore */
        if (prev == operation)
            return;
        
        /* If the previous one was a multi operation, just piggy back this one in */
        if (SEAHORSE_IS_MULTI_OPERATION (prev)) {
            seahorse_multi_operation_take (SEAHORSE_MULTI_OPERATION (prev), operation);
            return;
        }

        /* Otherwise disconnect old progress, replace with new */
        disconnect_progress (swidget, prev);
    }
        
    g_object_ref (operation);
    g_object_set_data_full (G_OBJECT (swidget), "operation", operation, 
                            (GDestroyNotify)g_object_unref);
    g_signal_connect (swidget, "destroy", 
                      G_CALLBACK (disconnect_progress), operation);
    
    if (!seahorse_operation_is_running (operation))
        operation_done (operation, swidget);

    operation_progress (operation, seahorse_operation_get_message (operation),
                        seahorse_operation_get_progress (operation), swidget);
    
    g_signal_connect (operation, "done", G_CALLBACK (operation_done), swidget);
    g_signal_connect (operation, "progress", G_CALLBACK (operation_progress), swidget);
}

SeahorseOperation*
seahorse_progress_status_get_operation (SeahorseWidget *swidget)
{
    return SEAHORSE_OPERATION (g_object_get_data (G_OBJECT (swidget), "operation"));
}

/* -----------------------------------------------------------------------------
 *  PROGRESS WINDOWS 
 */

static void
progress_operation_update (SeahorseOperation *operation, const gchar *message, 
                           gdouble fract, SeahorseWidget *swidget)
{
    GtkProgressBar *progress;
    GtkWidget *w;
    const gchar *t;
    
    w = glade_xml_get_widget (swidget->xml, "operation-details");
    g_return_if_fail (w != NULL);
    
    t = seahorse_operation_get_message (operation);
    gtk_label_set_text (GTK_LABEL (w), t ? t : "");
    
    progress = GTK_PROGRESS_BAR (glade_xml_get_widget (swidget->xml, "operation-bar"));
    g_return_if_fail (w != NULL);
    
    if (fract >= 0.0) {
        stop_pulse (progress);
        gtk_progress_bar_set_fraction (progress, fract);        
    } else { 
        start_pulse (progress);
    }
}

static void
progress_operation_cancel (GtkButton *button, SeahorseOperation *operation)
{
    if (seahorse_operation_is_running (operation))
        seahorse_operation_cancel (operation);
}

static void
progress_operation_done (SeahorseOperation *operation, SeahorseWidget *swidget)
{
    seahorse_widget_destroy (swidget);
}

static int
progress_delete_event (GtkWidget *widget, GdkEvent *event, 
                       SeahorseOperation *operation)
{
    /* When window close we simulate a cancel */
    progress_operation_cancel (NULL, operation);
    
    /* Allow window to close regardless of outcome */
    return TRUE;
}

static void        
progress_destroy (SeahorseWidget *swidget, SeahorseOperation *operation)
{
    /* 
     * Since we allow the window to close (user forces it) even when the 
     * operation is not complete, we have to take care to cleanup these
     * signal handlers. 
     */     
    g_signal_handlers_disconnect_by_func (operation, operation_done, swidget);
    g_signal_handlers_disconnect_by_func (operation, operation_progress, swidget);
}

static gboolean
progress_show (SeahorseOperation *operation)
{
    SeahorseWidget *swidget;
    GtkWidget *w;
    const gchar *title;
    gchar *t;

    if (!seahorse_operation_is_running (operation)) {
        /* Matches the ref in seahorse_progress_show */
        g_object_unref (operation);
        return FALSE;
    }
        
    swidget = seahorse_widget_new_allow_multiple ("progress", NULL);
    g_return_val_if_fail (swidget != NULL, FALSE);

    /* Release our reference on the operation when this window is destroyed */    
    g_object_set_data_full (G_OBJECT (swidget), "operation", operation, 
                            (GDestroyNotify)g_object_unref);    
        
    /* Setup the title */
    title = (const gchar*)g_object_get_data (G_OBJECT (operation), "progress-title");
    if (title) {
            
        /* The window title */
        w = glade_xml_get_widget (swidget->xml, swidget->name);
        g_return_val_if_fail (w != NULL, FALSE);
        gtk_window_set_title (GTK_WINDOW (w), title);
            
        /* The main message title */
        w = glade_xml_get_widget (swidget->xml, "operation-title");
        g_return_val_if_fail (w != NULL, FALSE);
        t = g_strdup_printf ("<b>%s</b>", title);
        gtk_label_set_markup (GTK_LABEL (w), t);
        g_free (t);
    }

    /* The details */        
    progress_operation_update (operation, NULL, 
                                seahorse_operation_get_progress (operation), swidget);
    g_signal_connect (operation, "progress", 
                      G_CALLBACK (progress_operation_update), swidget);

    /* Cancel events */
    glade_xml_signal_connect_data (swidget->xml, "cancel_clicked",
                                   G_CALLBACK (progress_operation_cancel), operation);
    g_signal_connect (seahorse_widget_get_toplevel (swidget), "delete_event",
                      G_CALLBACK (progress_delete_event), operation);
    
    /* Done and cleanup */
    w = glade_xml_get_widget (swidget->xml, swidget->name);
    g_signal_connect (w, "destroy", G_CALLBACK (progress_destroy), operation);
    g_signal_connect (operation, "done", G_CALLBACK (progress_operation_done), swidget);

    return FALSE;
}

void
seahorse_progress_show (SeahorseOperation *operation, const gchar *title, 
                        gboolean delayed)
{    
    /* Unref in the timeout callback */
    g_object_ref (operation);
    g_object_set_data_full (G_OBJECT (operation), "progress-title", 
                title ? g_strdup (title) : NULL, (GDestroyNotify)g_free);
    
    /* Show the progress, after one second */
    if (delayed)
        g_timeout_add (1000, (GSourceFunc)progress_show, operation);
    
    /* Right away */
    else
        progress_show (operation);
}
