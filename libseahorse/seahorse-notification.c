/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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
 
#include "config.h"

#include <gtk/gtk.h>

#include "seahorse-libdialogs.h"

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

static guint num_dialogs = 0;

static void
dialog_close (GtkDialog *dialog, gpointer dummy)
{
    if (num_dialogs >= 1)
        num_dialogs--;
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

gboolean
seahorse_notification_have ()
{   
    return num_dialogs > 0;
}

/* The non-libnotify version: Display a syucky dialog */
static void            
notification_display_fallback (const gchar *summary, const gchar* body, 
                               gboolean urgent, const gchar *icon)
{
    GtkWidget *hbox, *vbox;
    GtkWidget *label, *secondary_label, *image;
    GtkWidget *dialog;
    char *t;
    
    dialog = gtk_dialog_new_with_buttons (summary, NULL, GTK_DIALOG_NO_SEPARATOR, 
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    
    t = g_markup_printf_escaped ("<b><big>%s</big></b>", summary);
    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), t);
    g_free (t);
    
    secondary_label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (secondary_label), body);
    gtk_widget_set_size_request (secondary_label, 300, -1);
    
    if (icon)
        image = gtk_image_new_from_file (icon);
    else
        image = gtk_image_new_from_stock (urgent ? GTK_STOCK_DIALOG_WARNING : GTK_STOCK_DIALOG_INFO, 
                                          GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
  
    gtk_label_set_line_wrap  (GTK_LABEL (label), TRUE);
    gtk_label_set_selectable (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment   (GTK_MISC  (label), 0.0, 0.0);
  
    gtk_label_set_line_wrap  (GTK_LABEL (secondary_label), TRUE);
    gtk_label_set_selectable (GTK_LABEL (secondary_label), TRUE);
    gtk_misc_set_alignment   (GTK_MISC  (secondary_label), 0.0, 0.0);

    hbox = gtk_hbox_new (FALSE, 12);
    vbox = gtk_vbox_new (FALSE, 12);

    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), secondary_label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);

    gtk_container_set_border_width (GTK_CONTAINER (image->parent), 12);
    gtk_widget_show_all (hbox);    

    g_signal_connect (dialog, "close", G_CALLBACK (dialog_close), NULL);
    g_signal_connect (dialog, "response", G_CALLBACK (dialog_close), NULL);
    gtk_widget_show_all (GTK_WIDGET (dialog));
    
    num_dialogs++;
}

void 
seahorse_notification_display (const gchar *summary, const gchar *body,
                               gboolean urgent, const gchar *icon)
{
    gboolean res = FALSE;
#ifdef HAVE_LIBNOTIFY
    NotifyNotification *notif;
    GError *err = NULL;
    
    if (!notify_is_initted ())
        notify_init ("seahorse");

    notif = notify_notification_new (summary, body, icon, NULL);
    g_return_if_fail (notif != NULL);
        
    notify_notification_set_urgency (notif,  urgent ?
                                            NOTIFY_URGENCY_CRITICAL : 
                                            NOTIFY_URGENCY_NORMAL);
    
    res = notify_notification_show (notif, &err);
    if (!res) {
        g_warning ("couldn't show notification through libnotify: %s", err->message);
        g_clear_error (&err);
    }
    
#endif /* HAVE_LIBNOTIFY */ 
    
    /* If no display via libnotify, then display the dialog */
    if (!res)
        notification_display_fallback (summary, body, urgent, icon);
}
