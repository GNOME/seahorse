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

/* 
 * Much of the code is originally from pinentry-gtk2:
 * (C) by Albrecht Dreß 2004 unter the terms of the GNU Lesser General
 * Public License.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gnome.h>

#include "seahorse-agent.h"
#include "gtk-secure-entry.h"

#define HIG_SMALL      6        /* gnome hig small space in pixels */
#define HIG_LARGE     12        /* gnome hig large space in pixels */

/* Note: We only display one prompt at a time. */
static GtkWidget *g_current_win = NULL;
static GtkWidget *g_current_entry = NULL;

/* Whether we currently have a prompt or not */
gboolean
seahorse_agent_prompt_have ()
{
    return g_current_win != NULL;
}

/* Destroy any current prompts */
void
seahorse_agent_prompt_cleanup ()
{
    if (g_current_win) {
        gtk_widget_destroy (g_current_win);
        g_current_win = NULL;
        g_current_entry = NULL;
    }
}

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


/* constrain_size - constrain size of the window the window should not
 * shrink beyond the requisition, and should not grow vertically */
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
    geo.max_width = 10000;      /* limit is arbitrary, INT_MAX breaks other things */
    geo.min_height = geo.max_height = height;
    gtk_window_set_geometry_hints (GTK_WINDOW (win), NULL, &geo,
                                   GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
}

/* -----------------------------------------------------------------------------
 * PASSWORD PROMPT
 */

/* Called to close the prompt */
static void
prompt_done_dialog (SeahorseAgentPassReq *pr, gboolean ok)
{
    const gchar *pass = NULL;
    
    g_assert (g_current_win);
    gtk_widget_destroy (g_current_win);
    g_current_win = NULL;

    if (ok) {
      
        pass = seahorse_agent_cache_get (pr->id);
        g_assert (pass);
    }

    seahorse_agent_actions_donepass (pr, pass);
}

/* Called when the ok button is clicked */
static void
prompt_ok_button (GtkWidget *widget, gpointer data)
{
    SeahorseAgentPassReq *pr = (SeahorseAgentPassReq *) data;
    const char *s;

    g_assert (g_current_entry != NULL);
    s = gtk_secure_entry_get_text (GTK_SECURE_ENTRY (g_current_entry));
    seahorse_agent_cache_set (pr->id, s != NULL ? s : "", TRUE, TRUE);

    prompt_done_dialog (pr, TRUE);
}

/* Called when canceled */
static void
prompt_cancel_button (GtkWidget *widget, gpointer data)
{
    SeahorseAgentPassReq *pr = (SeahorseAgentPassReq *) data;
    prompt_done_dialog (pr, FALSE);
}

/* When enter is pressed in the entry, we simulate an ok */
static void
enter_callback (GtkWidget *widget, gpointer data)
{
    prompt_ok_button (widget, data);
}

/* grab_keyboard - grab the keyboard for maximum security */
static void
grab_keyboard (GtkWidget *win, GdkEvent * event, gpointer data)
{
    if (gdk_keyboard_grab (win->window, FALSE, gdk_event_get_time (event)))
        g_critical ("could not grab keyboard");
}

/* ungrab_keyboard - remove grab */
static void
ungrab_keyboard (GtkWidget *win, GdkEvent *event, gpointer data)
{
    gdk_keyboard_ungrab (gdk_event_get_time (event));
}

/* When window close we simulate a cancel */
static int
prompt_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    prompt_cancel_button (widget, data);
    return TRUE;
}

/* Initialize a password prompt */
static GtkWidget *
create_prompt_window (SeahorseAgentPassReq *pr)
{
    GtkWidget *w;
    GtkWidget *win;
    GtkWidget *box;
    GtkWidget *ebox;
    GtkWidget *wvbox;
    GtkWidget *chbox;
    GtkWidget *bbox;
    GtkAccelGroup *acc;
    gchar *msg;

    gboolean grab = TRUE;

    win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title (GTK_WINDOW (win), _("Passphrase"));
    acc = gtk_accel_group_new ();

    g_signal_connect (G_OBJECT (win), "delete_event",
                      G_CALLBACK (prompt_delete_event), pr);

    g_signal_connect (G_OBJECT (win), "size-request",
                      G_CALLBACK (constrain_size), NULL);

    g_signal_connect (G_OBJECT (win), grab ? "map-event" : "focus-in-event",
                      G_CALLBACK (grab_keyboard), NULL);
    g_signal_connect (G_OBJECT (win), grab ? "unmap-event" : "focus-out-event",
                      G_CALLBACK (ungrab_keyboard), NULL);

    gtk_window_add_accel_group (GTK_WINDOW (win), acc);

    wvbox = gtk_vbox_new (FALSE, HIG_LARGE * 2);
    gtk_container_add (GTK_CONTAINER (win), wvbox);
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
    if (pr->description) {
        msg = utf8_validate (pr->description);
        w = gtk_label_new (msg);
        g_free (msg);

        gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
        gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
        gtk_box_pack_start (GTK_BOX (box), w, TRUE, FALSE, 0);
    }

    /* Any error messsages */
    if (pr->errmsg) {
        GdkColor color = { 0, 0xffff, 0, 0 };

        msg = utf8_validate (pr->errmsg);
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
    if (pr->prompt) {
        msg = utf8_validate (pr->prompt);
        w = gtk_label_new (msg);
        g_free (msg);

        gtk_box_pack_start (GTK_BOX (ebox), w, FALSE, FALSE, 0);
    }

    g_current_entry = gtk_secure_entry_new ();
    gtk_widget_set_size_request (g_current_entry, 200, -1);
    g_signal_connect (G_OBJECT (g_current_entry), "activate",
                      G_CALLBACK (enter_callback), pr);

    gtk_box_pack_start (GTK_BOX (ebox), g_current_entry, TRUE, TRUE, 0);
    gtk_widget_grab_focus (g_current_entry);
    gtk_widget_show (g_current_entry);

    bbox = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing (GTK_BOX (bbox), 6);
    gtk_box_pack_start (GTK_BOX (wvbox), bbox, TRUE, FALSE, 0);

    w = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_container_add (GTK_CONTAINER (bbox), w);
    g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (prompt_cancel_button),
                      pr);

    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);

    w = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_container_add (GTK_CONTAINER (bbox), w);

    g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (prompt_ok_button), pr);
    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);
    gtk_widget_grab_default (w);
    g_signal_connect_object (G_OBJECT (g_current_entry), "focus_in_event",
                             G_CALLBACK (gtk_widget_grab_default), G_OBJECT (w), 0);

    gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_CENTER);
    gtk_widget_show_all (win);
    return win;
}

/* Show prompt for a password using the given request */
void
seahorse_agent_prompt_pass (SeahorseAgentPassReq *pr)
{
    g_assert (!seahorse_agent_prompt_have ());
    g_current_win = create_prompt_window (pr);
}


/* -----------------------------------------------------------------------------
 * Authorize Prompt
 */

/* Called when we want to close */
static void
auth_done_dialog (SeahorseAgentPassReq *pr, gboolean ok)
{
    g_assert (g_current_win);
    gtk_widget_destroy (g_current_win);
    g_current_win = NULL;

    seahorse_agent_actions_doneauth (pr, ok);
}

/* Called when ok button pressed */
static void
auth_ok_button (GtkWidget *widget, gpointer data)
{
    SeahorseAgentPassReq *pr = (SeahorseAgentPassReq *) data;
    auth_done_dialog (pr, TRUE);
}

/* Cancel button pressed */
static void
auth_cancel_button (GtkWidget * widget, gpointer data)
{
    SeahorseAgentPassReq *pr = (SeahorseAgentPassReq *) data;
    auth_done_dialog (pr, FALSE);
}

/* Simulate a cancel when window closed */
static int
auth_delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
    auth_cancel_button (widget, data);
    return TRUE;
}

/* Setup an authorize prompt */
static GtkWidget *
create_auth_window (SeahorseAgentPassReq *pr)
{
    GtkWidget *w;
    GtkWidget *win;
    GtkWidget *box;
    GtkWidget *wvbox;
    GtkWidget *chbox;
    GtkWidget *bbox;
    GtkAccelGroup *acc;
    gchar *msg;

    win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (win), _("Authorize Password Access"));
    acc = gtk_accel_group_new ();

    g_signal_connect (G_OBJECT (win), "delete_event",
                      G_CALLBACK (auth_delete_event), pr);

    g_signal_connect (G_OBJECT (win), "size-request",
                      G_CALLBACK (constrain_size), NULL);

    gtk_window_add_accel_group (GTK_WINDOW (win), acc);

    wvbox = gtk_vbox_new (FALSE, HIG_LARGE * 2);
    gtk_container_add (GTK_CONTAINER (win), wvbox);
    gtk_container_set_border_width (GTK_CONTAINER (wvbox), HIG_LARGE);

    chbox = gtk_hbox_new (FALSE, HIG_LARGE);
    gtk_box_pack_start (GTK_BOX (wvbox), chbox, FALSE, FALSE, 0);

    w = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG);
    gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (chbox), w, FALSE, FALSE, 0);

    box = gtk_vbox_new (FALSE, HIG_SMALL);
    gtk_box_pack_start (GTK_BOX (chbox), box, TRUE, TRUE, 0);

    if (pr->description) {
        msg = utf8_validate (pr->description);
        w = gtk_label_new (msg);
        g_free (msg);

        gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
        gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
        gtk_box_pack_start (GTK_BOX (box), w, TRUE, FALSE, 0);
    }

    w = gtk_label_new (_("The passphrase is cached in memory."));
    gtk_misc_set_alignment (GTK_MISC (w), 0.0, 0.5);
    gtk_label_set_line_wrap (GTK_LABEL (w), TRUE);
    gtk_box_pack_start (GTK_BOX (box), w, TRUE, FALSE, 0);

    bbox = gtk_hbutton_box_new ();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing (GTK_BOX (bbox), 6);
    gtk_box_pack_start (GTK_BOX (wvbox), bbox, TRUE, FALSE, 0);

    w = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
    gtk_container_add (GTK_CONTAINER (bbox), w);
    g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (auth_cancel_button), pr);

    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);

    w = gtk_button_new_from_stock (GTK_STOCK_OK);
    gtk_button_set_label (GTK_BUTTON (w), _("_Authorize"));
    gtk_button_set_use_underline (GTK_BUTTON (w), TRUE);
    gtk_container_add (GTK_CONTAINER (bbox), w);

    g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (auth_ok_button), pr);
    GTK_WIDGET_SET_FLAGS (w, GTK_CAN_DEFAULT);
    gtk_widget_grab_default (w);

    gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_CENTER);
    gtk_widget_show_all (win);
    return win;
}

/* Show an authorize prompt window */
void
seahorse_agent_prompt_auth (SeahorseAgentPassReq *pr)
{
    g_assert (!seahorse_agent_prompt_have ());
    g_current_win = create_auth_window (pr);
}
