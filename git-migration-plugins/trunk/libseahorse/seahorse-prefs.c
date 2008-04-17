/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Stefan Walter
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
#include <glib/gi18n.h>

#include <cryptui.h>
#include <cryptui-key-combo.h>
#include <cryptui-key-store.h>

#include "seahorse-prefs.h"
#include "seahorse-util.h"
#include "seahorse-check-button-control.h"
#include "seahorse-gconf.h"
#include "seahorse-gtkstock.h"
#include "seahorse-secure-entry.h"

/* From seahorse-prefs-cache.c */
void seahorse_prefs_cache (SeahorseWidget *widget);

/* From sehorse-prefs-keyrings.c */
void seahorse_prefs_keyrings (SeahorseWidget *widget);

/* -------------------------------------------------------------------------- */

static void
default_key_changed (GtkComboBox *combo, gpointer *data)
{
    const gchar *key = cryptui_key_combo_get_key (combo);
    seahorse_gconf_set_string (SEAHORSE_DEFAULT_KEY, key == 0 ? "" : key);
}

static void
gconf_notification (GConfClient *gclient, guint id, GConfEntry *entry, 
                    GtkComboBox *combo)
{
    const gchar *key = gconf_value_get_string (gconf_entry_get_value (entry));
    cryptui_key_combo_set_key (combo, key);
}

static void
remove_gconf_notification (GObject *obj, gpointer data)
{
    guint gconf_id = GPOINTER_TO_INT (data);
    seahorse_gconf_unnotify (gconf_id);
}

static gboolean 
signer_filter (CryptUIKeyset *ckset, const gchar *key, gpointer user_data)
{
    guint flags = cryptui_keyset_key_flags (ckset, key);
    return flags & CRYPTUI_FLAG_CAN_SIGN;
}

/**
 * seahorse_prefs_new
 * 
 * Create a new preferences window.
 * 
 * Returns: The preferences window.
 **/
SeahorseWidget *
seahorse_prefs_new (GtkWindow *parent)
{
    SeahorseWidget *swidget;
    CryptUIKeyset* keyset;
    CryptUIKeyStore* ckstore;
    GtkWidget *widget;
    guint gconf_id;
    
    swidget = seahorse_widget_new ("prefs", parent);
    
    widget = glade_xml_get_widget (swidget->xml, "encrypt-self");
    seahorse_check_button_gconf_attach (GTK_CHECK_BUTTON (widget), ENCRYPTSELF_KEY);
    
    widget = glade_xml_get_widget (swidget->xml, "signer-select");
    g_return_val_if_fail (widget != NULL, NULL);

    /* The Sign combo */
    keyset = cryptui_keyset_new ("openpgp", FALSE);
    ckstore = cryptui_key_store_new (keyset, FALSE, _("None. Prompt for a key."));
    cryptui_key_store_set_filter (ckstore, signer_filter, NULL);
    cryptui_key_combo_setup (GTK_COMBO_BOX (widget), ckstore);
    g_object_unref (ckstore);
    g_object_unref (keyset);

    cryptui_key_combo_set_key (GTK_COMBO_BOX (widget), seahorse_gconf_get_string (SEAHORSE_DEFAULT_KEY));
    g_signal_connect (widget, "changed", G_CALLBACK (default_key_changed), NULL);

    gconf_id = seahorse_gconf_notify (SEAHORSE_DEFAULT_KEY, 
                                      (GConfClientNotifyFunc)gconf_notification, GTK_COMBO_BOX (widget));
    g_signal_connect (widget, "destroy", G_CALLBACK (remove_gconf_notification), GINT_TO_POINTER (gconf_id));
    
#ifdef WITH_AGENT   
    seahorse_prefs_cache (swidget);
#else
    widget = glade_xml_get_widget (swidget->xml, "cache-tab");
    g_return_val_if_fail (GTK_IS_WIDGET (widget), swidget);
    seahorse_prefs_remove_tab (swidget, widget);
#endif

    seahorse_widget_show (swidget);
    return swidget;
}

/**
 * seahorse_prefs_add_tab
 * @swidget: The preferences window
 * 
 * Add a tab to the preferences window
 **/
void                
seahorse_prefs_add_tab (SeahorseWidget *swidget, GtkWidget *label, GtkWidget *tab)
{
    GtkWidget *widget;
    widget = glade_xml_get_widget (swidget->xml, "notebook");
    gtk_widget_show (label);
    gtk_notebook_prepend_page (GTK_NOTEBOOK (widget), tab, label);
}

void                
seahorse_prefs_select_tab (SeahorseWidget *swidget, GtkWidget *tab)
{
    GtkWidget *tabs;
    gint pos;
    
    g_return_if_fail (GTK_IS_WIDGET (tab));
    
    tabs = glade_xml_get_widget (swidget->xml, "notebook");
    g_return_if_fail (GTK_IS_NOTEBOOK (tabs));
    
    pos = gtk_notebook_page_num (GTK_NOTEBOOK (tabs), tab);
    if (pos != -1)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (tabs), pos);
}    

void 
seahorse_prefs_remove_tab (SeahorseWidget *swidget, GtkWidget *tab)
{
    GtkWidget *tabs;
    gint pos;
    
    g_return_if_fail (GTK_IS_WIDGET (tab));
    
    tabs = glade_xml_get_widget (swidget->xml, "notebook");
    g_return_if_fail (GTK_IS_NOTEBOOK (tabs));
    
    pos = gtk_notebook_page_num (GTK_NOTEBOOK (tabs), tab);
    if (pos != -1)
        gtk_notebook_remove_page (GTK_NOTEBOOK (tabs), pos);
}
