/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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

#include <libintl.h>
#include <gnome.h>

#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-op.h"
#include "seahorse-gpgmex.h"
#include "seahorse-context.h"
#include "seahorse-windows.h"
#include "seahorse-progress.h"
#include "seahorse-preferences.h"
#include "seahorse-server-source.h"
#include "seahorse-multi-source.h"
#include "seahorse-gconf.h"

static void 
sync_import_complete (SeahorseOperation *op, SeahorseKeySource *sksrc)
{
    GError *err = NULL;
    
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_steal_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't publish keys to server: %s"), 
                                    seahorse_gconf_get_string (PUBLISH_TO_KEY));
    }    
    
    g_object_unref (sksrc);
}

static void
sync_export_complete (SeahorseOperation *op, SeahorseKeySource *sksrc)
{
    GError *err = NULL;

    if (seahorse_operation_is_successful (op)) {
        SeahorseKeySource *lsrc;
        gpgme_data_t data;
        
        data = seahorse_operation_get_result (op);
        g_return_if_fail (data != NULL);

        /* Now import it into the local key source */
        g_object_get (sksrc, "local-source", &lsrc, NULL);
        if (!seahorse_key_source_import_sync (lsrc, data, &err))
            seahorse_util_handle_error (err, "Couldn't import keys");
        
        /* |data| is owned by the operation, no need to free */
        
    } else {
        gchar *keyserver;
        g_object_get (sksrc, "key-server", &keyserver, NULL);

        seahorse_operation_steal_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't retrieve keys from server: %s"), 
                                    keyserver);
        g_free (keyserver);
    }    
    
    g_object_unref (sksrc);
}

static void
ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    SeahorseKeySource *lsksrc;
    SeahorseKeySource *sksrc;
    SeahorseContext *sctx;
    SeahorseMultiOperation *mop;
    SeahorseOperation *op;
    GError *err = NULL;
    gpgme_error_t gerr;
    gchar *keyserver;
    GSList *ks, *l;
    GList *keys;
    
    keys = (GList*)g_object_get_data (G_OBJECT (swidget), "publish-keys");
    sctx = swidget->sctx;
    
    seahorse_widget_destroy (swidget);
    
    if (!keys)
        return;

    /* This should be the default key source */
    lsksrc = seahorse_context_get_key_source (swidget->sctx);
    g_return_if_fail (lsksrc != NULL);

    mop = seahorse_multi_operation_new ();

    /* And now syncing keys from the servers */
    ks = seahorse_gconf_get_string_list (KEYSERVER_KEY);
	ks = seahorse_server_source_purge_keyservers (ks);
	
    for (l = ks; l; l = g_slist_next (l)) {

        sksrc = SEAHORSE_KEY_SOURCE (seahorse_server_source_new (lsksrc, (const gchar*)(l->data), NULL));
        op = seahorse_key_source_export (sksrc, keys, FALSE, NULL);
        g_return_if_fail (op != NULL);

        g_signal_connect (op, "done", G_CALLBACK (sync_export_complete), sksrc);
        seahorse_multi_operation_add (mop, op);
    }
    seahorse_util_string_slist_free (ks);
    
    /* Publishing keys online */    
    keyserver = seahorse_gconf_get_string (PUBLISH_TO_KEY);
    if (keyserver && keyserver[0]) {
        
        gchar *exported;
        gpgme_data_t data;

        /* Export all the necessary keys from our local keyring */
        exported = seahorse_op_export_text (keys, FALSE, &err);
        if (!exported) {
            seahorse_util_handle_error (err, _("Couldn't export keys"));
        } else {
            /* New GPGME data which copies original text */
            gerr = gpgme_data_new_from_mem (&data, exported, strlen (exported), 0);
            g_return_if_fail (GPG_IS_OK (gerr));

            sksrc = SEAHORSE_KEY_SOURCE (seahorse_server_source_new (lsksrc, keyserver, NULL));
            op = seahorse_key_source_import (sksrc, data);
            g_return_if_fail (op != NULL);
            
            gpgme_data_release (data);
            g_free (exported);
            
            g_signal_connect (op, "done", G_CALLBACK (sync_import_complete), sksrc);
            seahorse_multi_operation_add (mop, op);
        }
        
    }

    g_free (keyserver);
    
    /* Show the progress window if necessary */
    if (!seahorse_operation_is_done (SEAHORSE_OPERATION (mop))) 
        seahorse_progress_show (sctx, SEAHORSE_OPERATION (mop), 
                                _("Syncing keys..."), FALSE);
}

static void
configure_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    seahorse_preferences_show (swidget->sctx, 3);
}

static void
update_message (SeahorseWidget *swidget)
{
    GtkWidget *w, *w2;
    gchar *t;
    
    w = glade_xml_get_widget (swidget->xml, "publish-message");
    w2 = glade_xml_get_widget (swidget->xml, "sync-message");

    t = seahorse_gconf_get_string (PUBLISH_TO_KEY);
    if (t && t[0]) {
        gtk_widget_show (w);
        gtk_widget_hide (w2);
    } else {
        gtk_widget_hide (w);
        gtk_widget_show (w2);
    }
    g_free (t);
}

static void
gconf_notify (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
    SeahorseWidget *swidget;

    if (g_str_equal (PUBLISH_TO_KEY, gconf_entry_get_key (entry))) {
        swidget = SEAHORSE_WIDGET (data);
        update_message (swidget);
    }
}

static void
unhook_notification (GtkWidget *widget, gpointer data)
{
    guint notify_id = GPOINTER_TO_INT (data);
    seahorse_gconf_unnotify (notify_id);
}

/**
 * seahorse_keyserver_sync_show
 * @sctx: The SeahorseContext
 * @keys: The keys to synchronize
 * 
 * Shows a synchronize window.
 * 
 * Returns the new window.
 **/
GtkWindow*
seahorse_keyserver_sync_show (SeahorseContext *sctx, GList *keys)
{	
	SeahorseWidget *swidget;
    GtkWindow *win;
    GtkWidget *w;
    guint n, notify_id;
    gchar *t;
    
	swidget = seahorse_widget_new ("keyserver-sync", sctx);
	g_return_val_if_fail (swidget != NULL, NULL);
    
    win = GTK_WINDOW (glade_xml_get_widget (swidget->xml, swidget->name));
    
    /* The details message */
    n = g_list_length (keys);
    t = g_strdup_printf (ngettext ("<b>%d key is selected for syncing</b>", 
                                   "<b>%d keys are selected for syncing</b>", n), n);
    
    w = glade_xml_get_widget (swidget->xml, "detail-message");
    g_return_val_if_fail (swidget != NULL, win);
    gtk_label_set_markup (GTK_LABEL (w), t);
    g_free (t);
            
    /* The right help message */
    update_message (swidget);
    notify_id = seahorse_gconf_notify (PUBLISH_TO_KEY, gconf_notify, swidget);
    g_signal_connect (win, "destroy", G_CALLBACK (unhook_notification), 
                      GINT_TO_POINTER (notify_id));

    g_object_set_data (G_OBJECT (swidget), "publish-keys", keys);
    
	glade_xml_signal_connect_data (swidget->xml, "ok_clicked",
		                           G_CALLBACK (ok_clicked), swidget);
	glade_xml_signal_connect_data (swidget->xml, "configure_clicked",
		                           G_CALLBACK (configure_clicked), swidget);

    return win;
}
