/*
 * Seahorse
 *
 * Copyright (C) 2005 Stefan Walter
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

#include <config.h>

#include <glib/gi18n.h>

#include "seahorse-context.h"
#include "seahorse-keyserver-sync.h"
#include "seahorse-object.h"
#include "seahorse-progress.h"
#include "seahorse-preferences.h"
#include "seahorse-servers.h"
#include "seahorse-transfer-operation.h"
#include "seahorse-util.h"
#include "seahorse-widget.h"
#include "seahorse-windows.h"

static void 
sync_import_complete (SeahorseOperation *op, SeahorseSource *sksrc)
{
	GError *error = NULL;
	gchar *publish_to;

	if (!seahorse_operation_is_successful (op)) {
		seahorse_operation_copy_error (op, &error);
		publish_to = g_settings_get_string (seahorse_context_settings (NULL),
		                                    "server-publish-to");
		seahorse_util_handle_error (error, _("Couldn't publish keys to server"),
		                            publish_to);
		g_free (publish_to);
		g_clear_error (&error);
	}
}

static void
sync_export_complete (SeahorseOperation *op, SeahorseSource *sksrc)
{
    GError *err = NULL;
    gchar *keyserver;

    if (!seahorse_operation_is_successful (op)) {
        g_object_get (sksrc, "key-server", &keyserver, NULL);

        seahorse_operation_copy_error (op, &err);
        seahorse_util_handle_error (err, _("Couldn't retrieve keys from server: %s"), 
                                    keyserver);
        g_clear_error (&err);
        g_free (keyserver);
    }    
}

G_MODULE_EXPORT void
on_sync_ok_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    GList *keys;
    
    keys = (GList*)g_object_get_data (G_OBJECT (swidget), "publish-keys");
    keys = g_list_copy (keys);
    
    seahorse_widget_destroy (swidget);
   
    seahorse_keyserver_sync (keys);
    g_list_free (keys);
}

G_MODULE_EXPORT void
on_sync_configure_clicked (GtkButton *button, SeahorseWidget *swidget)
{
    seahorse_preferences_show (GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name)), "keyserver-tab");
}

static void
update_message (SeahorseWidget *swidget)
{
	GtkWidget *widget;
	GtkWidget *widget2;
	GtkWidget *sync_button;
	gchar *text;

	widget = seahorse_widget_get_widget (swidget, "publish-message");
	widget2 = seahorse_widget_get_widget (swidget, "sync-message");
	sync_button = seahorse_widget_get_widget (swidget, "sync-button");

	text = g_settings_get_string (seahorse_context_settings (NULL),
	                              "server-publish-to");
	if (text && text[0]) {
		gtk_widget_show (widget);
		gtk_widget_hide (widget2);
		gtk_widget_set_sensitive (sync_button, TRUE);
	} else {
		gtk_widget_hide (widget);
		gtk_widget_show (widget2);
		gtk_widget_set_sensitive (sync_button, FALSE);
	}
	g_free (text);
}

static void
on_settings_publish_to_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
	SeahorseWidget *swidget = SEAHORSE_WIDGET (user_data);
	update_message (swidget);
}

/**
 * seahorse_keyserver_sync_show
 * @keys: The keys to synchronize
 * 
 * Shows a synchronize window.
 * 
 * Returns the new window.
 **/
GtkWindow*
seahorse_keyserver_sync_show (GList *keys, GtkWindow *parent)
{
    SeahorseWidget *swidget;
    GtkWindow *win;
    GtkWidget *w;
    guint n;
    gchar *t;
    
    swidget = seahorse_widget_new_allow_multiple ("keyserver-sync", parent);
    g_return_val_if_fail (swidget != NULL, NULL);
    
    win = GTK_WINDOW (seahorse_widget_get_widget (swidget, swidget->name));
    
    /* The details message */
    n = g_list_length (keys);
    t = g_strdup_printf (ngettext ("<b>%d key is selected for synchronizing</b>", 
                                   "<b>%d keys are selected for synchronizing</b>", n), n);
    
    w = GTK_WIDGET (seahorse_widget_get_widget (swidget, "detail-message"));
    g_return_val_if_fail (swidget != NULL, win);
    gtk_label_set_markup (GTK_LABEL (w), t);
    g_free (t);

    /* The right help message */
    update_message (swidget);
    g_signal_connect_object (seahorse_context_settings (NULL), "changed::server-publish-to",
                             G_CALLBACK (on_settings_publish_to_changed), swidget, 0);

    keys = g_list_copy (keys);
    g_return_val_if_fail (!keys || SEAHORSE_IS_OBJECT (keys->data), win);
    g_object_set_data_full (G_OBJECT (swidget), "publish-keys", keys, 
                            (GDestroyNotify)g_list_free);
    
    return win;
}

void
seahorse_keyserver_sync (GList *keys)
{
    SeahorseSource *sksrc;
    SeahorseSource *lsksrc;
    SeahorseMultiOperation *mop;
    SeahorseOperation *op;
    gchar *keyserver;
    gchar **keyservers;
    GList *k;
    GSList *keyids = NULL;
    guint i;

    if (!keys)
        return;
    
    g_assert (SEAHORSE_IS_OBJECT (keys->data));
    
    /* Build a keyid list */
    for (k = keys; k; k = g_list_next (k)) 
        keyids = g_slist_prepend (keyids, 
                    GUINT_TO_POINTER (seahorse_object_get_id (SEAHORSE_OBJECT (k->data))));

    mop = seahorse_multi_operation_new ();

    /* And now synchronizing keys from the servers */
    keyservers = seahorse_servers_get_uris ();
    for (i = 0; keyservers[i] != NULL; i++) {
        sksrc = seahorse_context_remote_source (SCTX_APP (), keyservers[i]);

        /* This can happen if the URI scheme is not supported */
        if (sksrc == NULL)
            continue;
        
        lsksrc = seahorse_context_find_source (SCTX_APP (), 
                        seahorse_source_get_tag (sksrc), SEAHORSE_LOCATION_LOCAL);
        
        if (lsksrc) {
            op = seahorse_transfer_operation_new (_("Synchronizing keys"), sksrc, lsksrc, keyids);
            g_return_if_fail (op != NULL);

            seahorse_multi_operation_take (mop, op);
            seahorse_operation_watch (op, (SeahorseDoneFunc) sync_export_complete, sksrc, NULL, NULL);
        }
    }

    g_strfreev (keyservers);

    /* Publishing keys online */
    keyserver = g_settings_get_string (seahorse_context_settings (NULL),
                                       "server-publish-to");
    if (keyserver && keyserver[0]) {
        sksrc = seahorse_context_remote_source (SCTX_APP (), keyserver);

        /* This can happen if the URI scheme is not supported */
        if (sksrc != NULL) {

            op = seahorse_context_transfer_objects (SCTX_APP (), keys, sksrc);
            g_return_if_fail (sksrc != NULL);

            seahorse_multi_operation_take (mop, op);
            seahorse_operation_watch (op, (SeahorseDoneFunc) sync_import_complete, sksrc, NULL, NULL);

        }
    }

    g_slist_free (keyids);
    g_free (keyserver);
    
    /* Show the progress window if necessary */
    seahorse_progress_show (SEAHORSE_OPERATION (mop), _("Synchronizing keys..."), FALSE);
    g_object_unref (mop);
}
