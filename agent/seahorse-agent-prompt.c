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
#include "seahorse-check-button-control.h"
#include "seahorse-libdialogs.h"

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

static void
passphrase_response (GtkDialog *dialog, gint response, SeahorseAgentPassReq *pr)
{
    const char *s;

    switch (response) {
    case GTK_RESPONSE_ACCEPT:
        s = seahorse_passphrase_prompt_get (dialog);
        seahorse_agent_cache_set (pr->id, s != NULL ? s : "", TRUE, TRUE);
        prompt_done_dialog (pr, TRUE);
        break;
    default:
        prompt_done_dialog (pr, FALSE);
        break;
    };
}

void
seahorse_agent_prompt_pass (SeahorseAgentPassReq *pr)
{
    GtkDialog *dialog;

    g_assert (!seahorse_agent_prompt_have ());
    
    dialog = seahorse_passphrase_prompt_show (NULL, pr->description, 
                                              pr->prompt, pr->errmsg);
    g_signal_connect (dialog, "response", G_CALLBACK (passphrase_response), pr);
    g_current_win = GTK_WIDGET (dialog);
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
    
    w = seahorse_check_button_control_new(_("Always ask me before using a cached passphrase"),
                                          SETTING_AUTH);
    
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
    gtk_window_set_keep_above(GTK_WINDOW (win), TRUE);
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
