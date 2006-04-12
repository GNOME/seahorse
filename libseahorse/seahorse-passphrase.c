/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "config.h"
#include <gnome.h>
#include <glade/glade-xml.h>

#include "seahorse-gpgmex.h"
#include "seahorse-libdialogs.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-secure-entry.h"

#define HIG_SMALL      6        /* gnome hig small space in pixels */
#define HIG_LARGE     12        /* gnome hig large space in pixels */

/* Convert passed text to utf-8 if not valid */
static gchar *
utf8_validate (const gchar *text)
{
    gchar *result;

    if (!text)
        return NULL;

    if (g_utf8_validate (text, -1, NULL))
        return g_strdup (text);

    result = g_locale_to_utf8 (text, -1, NULL, NULL, NULL);
    if (!result) {
        gchar *p;

        result = p = g_strdup (text);
        while (!g_utf8_validate (p, -1, (const gchar **) &p))
            *p = '?';
    }
    return result;
}

static void
grab_keyboard (GtkWidget *win, GdkEvent * event, gpointer data)
{
#ifndef _DEBUG
    if (gdk_keyboard_grab (win->window, FALSE, gdk_event_get_time (event)))
        g_critical ("could not grab keyboard");
#endif
}

/* ungrab_keyboard - remove grab */
static void
ungrab_keyboard (GtkWidget *win, GdkEvent *event, gpointer data)
{
#ifndef _DEBUG
    gdk_keyboard_ungrab (gdk_event_get_time (event));
#endif
}

/* When enter is pressed in the entry, we simulate an ok */
static void
enter_callback (GtkWidget *widget, GtkDialog *dialog)
{
    gtk_dialog_response (dialog, GTK_RESPONSE_ACCEPT);
}


static void
constrain_size (GtkWidget *win, GtkRequisition *req, gpointer data)
{
    static gint width, height;
    GdkGeometry geo;

    if (req->width == width && req->height == height)
        return;

    width = req->width;
    height = req->height;
    geo.min_width = width;
    geo.max_width = 10000;  /* limit is arbitrary, INT_MAX breaks other things */
    geo.min_height = geo.max_height = height;
    gtk_window_set_geometry_hints (GTK_WINDOW (win), NULL, &geo,
                                   GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
}

GtkDialog*
seahorse_passphrase_prompt_show (const gchar *title, const gchar *description, 
                                 const gchar *prompt, const gchar *errmsg)
{
    SeahorseSecureEntry *entry;
    GtkDialog *dialog;
    GtkWidget *w;
    GtkWidget *box;
    GtkWidget *ebox;
    GtkWidget *wvbox;
    GtkWidget *chbox;
    gchar *msg;
    
    if (!title)
        title = _("Passphrase");

    if (!prompt)
        prompt = _("Password:"); 
    
    w = gtk_dialog_new_with_buttons (title, NULL, GTK_DIALOG_NO_SEPARATOR, NULL);
    dialog = GTK_DIALOG (w);

    g_signal_connect (G_OBJECT (dialog), "size-request",
                      G_CALLBACK (constrain_size), NULL);

    g_signal_connect (G_OBJECT (dialog), "map-event", G_CALLBACK (grab_keyboard), NULL);
    g_signal_connect (G_OBJECT (dialog), "unmap-event", G_CALLBACK (ungrab_keyboard), NULL);

    wvbox = gtk_vbox_new (FALSE, HIG_LARGE * 2);
    gtk_container_add (GTK_CONTAINER (dialog->vbox), wvbox);
    gtk_container_set_border_width (GTK_CONTAINER (wvbox), HIG_LARGE);

    chbox = gtk_hbox_new (FALSE, HIG_LARGE);
    gtk_box_pack_start (GTK_BOX (wvbox), chbox, FALSE, FALSE, 0);

    /* The image */
    w = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (chbox), w, FALSE, FALSE, 0);

    box = gtk_vbox_new (FALSE, HIG_SMALL);
    gtk_box_pack_start (GTK_BOX (chbox), box, TRUE, TRUE, 0);

    /* The description text */
    if (description) {
        msg = utf8_validate (description);
        w = gtk_label_new (msg);
        g_free (msg);

        gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
        gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
        gtk_box_pack_start (GTK_BOX (box), w, TRUE, FALSE, 0);
    }

    /* Any error messsages */
    if (errmsg) {
        GdkColor color = { 0, 0xffff, 0, 0 };

        msg = utf8_validate (errmsg);
        w = gtk_label_new (msg);
        g_free (msg);

        gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
        gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
        gtk_box_pack_start (GTK_BOX (box), w, TRUE, FALSE, 0);
        gtk_widget_modify_fg (w, GTK_STATE_NORMAL, &color);
    }

    ebox = gtk_hbox_new (FALSE, HIG_SMALL);
    gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, FALSE, 0);

    /* Prompt goes before the entry */
    msg = utf8_validate (prompt);
    w = gtk_label_new (msg);
    g_free (msg);

    gtk_box_pack_start (GTK_BOX (ebox), w, FALSE, FALSE, 0);

    entry = SEAHORSE_SECURE_ENTRY (seahorse_secure_entry_new ());
    gtk_widget_set_size_request (GTK_WIDGET (entry), 200, -1);
    g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (enter_callback), dialog);

    gtk_box_pack_start (GTK_BOX (ebox), GTK_WIDGET (entry), TRUE, TRUE, 0);
    gtk_widget_grab_focus (GTK_WIDGET (entry));
    gtk_widget_show (GTK_WIDGET (entry));

    g_object_set_data (G_OBJECT (dialog), "secure-entry", entry);

    w = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_dialog_add_action_widget (dialog, w, GTK_RESPONSE_REJECT);
    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);

    w = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_dialog_add_action_widget (dialog, w, GTK_RESPONSE_ACCEPT);
    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);
    g_signal_connect_object (G_OBJECT (entry), "focus_in_event",
                             G_CALLBACK (gtk_widget_grab_default), G_OBJECT (w), 0);
    gtk_widget_grab_default (w);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_keep_above(GTK_WINDOW (dialog), TRUE);
    gtk_window_present (GTK_WINDOW (dialog));
    gtk_widget_show_all (GTK_WIDGET (dialog));

    return dialog;
}

const gchar*
seahorse_passphrase_prompt_get (GtkDialog *dialog)
{
    SeahorseSecureEntry *entry;

    entry = SEAHORSE_SECURE_ENTRY (g_object_get_data (G_OBJECT (dialog), "secure-entry"));
    return seahorse_secure_entry_get_text (entry);
}

gpgme_error_t
seahorse_passphrase_get (gconstpointer dummy, const gchar *passphrase_hint, 
                         const char* passphrase_info, int flags, int fd)
{
    GtkDialog *dialog;
    gpgme_error_t err;
    gchar **split_uid = NULL;
    gchar *label = NULL;
    gchar *errmsg = NULL;
    const gchar *pass;
    
    if (passphrase_info && strlen(passphrase_info) < 16)
        flags |= SEAHORSE_PASS_NEW;
    
    if (passphrase_hint)
        split_uid = g_strsplit (passphrase_hint, " ", 2);

    if (flags & SEAHORSE_PASS_BAD) 
        errmsg = g_strdup_printf (_("Wrong passphrase."));
    
    if (split_uid && split_uid[0] && split_uid[1]) {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup_printf (_("Enter new passphrase for '%s'"), split_uid[1]);
        else 
            label = g_strdup_printf (_("Enter passphrase for '%s'"), split_uid[1]);
    } else {
        if (flags & SEAHORSE_PASS_NEW) 
            label = g_strdup (_("Enter new passphrase"));
        else 
            label = g_strdup (_("Enter passphrase"));
    }

    g_strfreev (split_uid);

    dialog = seahorse_passphrase_prompt_show (NULL, label, NULL, errmsg);
    g_free (label);
    g_free (errmsg);
    
    switch (gtk_dialog_run (dialog)) {
    case GTK_RESPONSE_ACCEPT:
        pass = seahorse_passphrase_prompt_get (dialog);
        seahorse_util_printf_fd (fd, "%s\n", pass);
        err = GPG_OK;
        break;
    default:
        err = GPG_E (GPG_ERR_CANCELED);
        break;
    };
    
    gtk_widget_destroy (GTK_WIDGET (dialog));
    return err;
}
