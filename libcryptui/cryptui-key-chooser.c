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

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "cryptui-key-list.h"
#include "cryptui-key-combo.h"
#include "cryptui-key-chooser.h"

enum {
    PROP_0,
    PROP_KEYSET
};

struct _CryptUIKeyChooserPriv {
    gboolean                initialized;
    CryptUIKeyset           *ckset;
    CryptUIKeyStore         *ckstore;
    GtkTreeView             *keylist;
    GtkComboBox             *keycombo;
    
    GtkComboBox             *filtermode;
    GtkEntry                *filtertext;
};

G_DEFINE_TYPE (CryptUIKeyChooser, cryptui_key_chooser, GTK_TYPE_VBOX);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gboolean 
recipients_filter (CryptUIKeyset *ckset, const gchar *key, gpointer user_data)
{
    guint flags = cryptui_keyset_key_flags (ckset, key);
    return flags & CRYPTUI_FLAG_CAN_ENCRYPT;
}

static gboolean 
signer_filter (CryptUIKeyset *ckset, const gchar *key, gpointer user_data)
{
    guint flags = cryptui_keyset_key_flags (ckset, key);
    return flags & CRYPTUI_FLAG_CAN_SIGN;
}

static void
filtertext_changed (GtkWidget *widget, CryptUIKeyChooser *chooser)
{
    const gchar *text = gtk_entry_get_text (chooser->priv->filtertext);
    g_object_set (chooser->priv->ckstore, "search", text, NULL);
}

static void
filtertext_activate (GtkEntry *entry, CryptUIKeyChooser *chooser)
{
    gtk_widget_grab_focus (GTK_WIDGET (chooser->priv->keylist));
}

static void
filtermode_changed (GtkWidget *widget, CryptUIKeyChooser *chooser)
{
    gint active = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));
    if (active >= 0)
        g_object_set (chooser->priv->ckstore, "mode", active, NULL);    
}


static void
construct_recipients (CryptUIKeyChooser *chooser, GtkBox *box)
{
    GtkWidget *scroll;
    GtkWidget *label;
    GtkWidget *hbox;

    /* Top filter box */
    hbox = gtk_hbox_new (FALSE, 12);
    
    /* Filter Combo */
    chooser->priv->filtermode = GTK_COMBO_BOX (gtk_combo_box_new_text ());
    gtk_combo_box_append_text (chooser->priv->filtermode, _("All Keys"));
    gtk_combo_box_append_text (chooser->priv->filtermode, _("Selected Recipients"));
    gtk_combo_box_append_text (chooser->priv->filtermode, _("Search Results"));
    gtk_combo_box_set_active (chooser->priv->filtermode, 0);
    g_signal_connect (chooser->priv->filtermode, "changed", 
                      G_CALLBACK (filtermode_changed), chooser);
    gtk_widget_set_size_request (GTK_WIDGET (chooser->priv->filtermode), 140, -1);
    gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (chooser->priv->filtermode));
    gtk_box_set_child_packing (GTK_BOX (hbox), GTK_WIDGET (chooser->priv->filtermode), 
                               FALSE, TRUE, 0, GTK_PACK_START);
    
    /* Filter Label */
    label = gtk_label_new (_("Search _for:"));
    gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
    gtk_container_add (GTK_CONTAINER (hbox), label);
    gtk_box_set_child_packing (GTK_BOX (hbox), label, 
                               TRUE, TRUE, 0, GTK_PACK_START);

    /* Filter Entry */
    chooser->priv->filtertext = GTK_ENTRY (gtk_entry_new ());
    gtk_entry_set_max_length (chooser->priv->filtertext, 256);
    gtk_widget_set_size_request (GTK_WIDGET (chooser->priv->filtertext), 140, -1);
    g_signal_connect (chooser->priv->filtertext, "changed", 
                      G_CALLBACK (filtertext_changed), chooser);
    g_signal_connect (chooser->priv->filtertext, "activate", 
                      G_CALLBACK (filtertext_activate), chooser);
    gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (chooser->priv->filtertext));
    gtk_box_set_child_packing (GTK_BOX (hbox), GTK_WIDGET (chooser->priv->filtertext), 
                               FALSE, TRUE, 0, GTK_PACK_START);
    
    /* Add Filter box */
    gtk_container_add (GTK_CONTAINER (box), hbox);
    gtk_box_set_child_packing (GTK_BOX (box), hbox, 
                               FALSE, TRUE, 0, GTK_PACK_START);
    
    chooser->priv->ckstore = cryptui_key_store_new (chooser->priv->ckset, TRUE, NULL);
    cryptui_key_store_set_filter (chooser->priv->ckstore, recipients_filter, NULL);
    
    /* Main Key list */
    chooser->priv->keylist = cryptui_key_list_new (chooser->priv->ckstore, 
                                                   CRYPTUI_KEY_LIST_CHECKS);
    gtk_tree_view_set_enable_search (GTK_TREE_VIEW (chooser->priv->keylist), FALSE);
    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), 
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (chooser->priv->keylist));
    gtk_container_add (GTK_CONTAINER (box), scroll);
    gtk_box_set_child_packing (box, scroll, TRUE, TRUE, 0, GTK_PACK_START);
}

static void
construct_signer (CryptUIKeyChooser *chooser, GtkBox *box)
{
    CryptUIKeyStore *ckstore;
    GtkWidget *hbox;
    GtkWidget *label;

    /* Top filter box */
    hbox = gtk_hbox_new (FALSE, 12);
    
    /* Sign Label */
    label = gtk_label_new (_("_Sign message as:"));
    gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
    gtk_container_add (GTK_CONTAINER (hbox), label);
    gtk_box_set_child_packing (GTK_BOX (hbox), label, 
                               FALSE, TRUE, 0, GTK_PACK_START);
    
    /* TODO: HIG and beautification */
    
    /* TODO: When only one key is present this should be a checkbox
       ie: 'Sign this Message (as 'key name') */

    /* The Sign combo */
    ckstore = cryptui_key_store_new (chooser->priv->ckset, TRUE, _("None (Don't Sign)"));
    cryptui_key_store_set_filter (ckstore, signer_filter, NULL);
    chooser->priv->keycombo = cryptui_key_combo_new (ckstore);
    g_object_unref (ckstore);
    gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (chooser->priv->keycombo));
    gtk_box_set_child_packing (GTK_BOX (hbox), GTK_WIDGET (chooser->priv->keycombo), 
                               TRUE, TRUE, 0, GTK_PACK_START);
                               
    /* Add it in */
    gtk_container_add (GTK_CONTAINER (box), hbox);
    gtk_box_set_child_packing (box, hbox, FALSE, TRUE, 0, GTK_PACK_START);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
cryptui_key_chooser_init (CryptUIKeyChooser *chooser)
{
    /* init private vars */
    chooser->priv = g_new0 (CryptUIKeyChooserPriv, 1);
}

static GObject*  
cryptui_key_chooser_constructor (GType type, guint n_props, GObjectConstructParam* props)
{
    GObject *obj = G_OBJECT_CLASS (cryptui_key_chooser_parent_class)->constructor (type, n_props, props);
    CryptUIKeyChooser *chooser = CRYPTUI_KEY_CHOOSER (obj);
    
    /* Set the spacing for this box */
    gtk_box_set_spacing (GTK_BOX (obj), 6);
    gtk_container_set_border_width (GTK_CONTAINER (obj), 6);
    
    /* Add the various objects now */
    construct_recipients (chooser, GTK_BOX (obj));
    construct_signer (chooser, GTK_BOX (obj));
    
    /* TODO: Fill in the default selection */

    chooser->priv->initialized = TRUE;
    return obj;
}

/* dispose of all our internal references */
static void
cryptui_key_chooser_dispose (GObject *gobject)
{
    CryptUIKeyChooser *chooser = CRYPTUI_KEY_CHOOSER (gobject);  

    if (chooser->priv->ckset)
        g_object_unref (chooser->priv->ckset);        
    chooser->priv->ckset = NULL;
    
    if (chooser->priv->ckstore)
        g_object_unref (chooser->priv->ckstore);
    chooser->priv->ckstore = NULL;
    
    G_OBJECT_CLASS (cryptui_key_chooser_parent_class)->dispose (gobject);
}

static void
cryptui_key_chooser_finalize (GObject *gobject)
{
    CryptUIKeyChooser *chooser = CRYPTUI_KEY_CHOOSER (gobject);
    
    g_assert (chooser->priv->ckset == NULL);
    g_free (chooser->priv);
    
    G_OBJECT_CLASS (cryptui_key_chooser_parent_class)->finalize (gobject);
}

static void
cryptui_key_chooser_set_property (GObject *gobject, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    CryptUIKeyChooser *chooser = CRYPTUI_KEY_CHOOSER (gobject);
    
    switch (prop_id) {
    case PROP_KEYSET:
        g_assert (chooser->priv->ckset == NULL);
        chooser->priv->ckset = g_value_get_object (value);
        g_object_ref (chooser->priv->ckset);
        break;
        
    default:
        break;
    }
}

static void
cryptui_key_chooser_get_property (GObject *gobject, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    CryptUIKeyChooser *chooser = CRYPTUI_KEY_CHOOSER (gobject);

    switch (prop_id) {
    case PROP_KEYSET:
        g_value_set_object (value, chooser->priv->ckset);
        break;
    
    default:
        break;
    }
}

static void
cryptui_key_chooser_class_init (CryptUIKeyChooserClass *klass)
{
    GObjectClass *gclass;

    cryptui_key_chooser_parent_class = g_type_class_peek_parent (klass);
    gclass = G_OBJECT_CLASS (klass);

    gclass->constructor = cryptui_key_chooser_constructor;
    gclass->dispose = cryptui_key_chooser_dispose;
    gclass->finalize = cryptui_key_chooser_finalize;
    gclass->set_property = cryptui_key_chooser_set_property;
    gclass->get_property = cryptui_key_chooser_get_property;
    
    g_object_class_install_property (gclass, PROP_KEYSET,
        g_param_spec_object ("keyset", "CryptUI Keyset", "Current CryptUI Key Source to use",
                             CRYPTUI_TYPE_KEYSET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}


/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CryptUIKeyChooser*
cryptui_key_chooser_new (CryptUIKeyset *ckset)
{
    GObject *obj = g_object_new (CRYPTUI_TYPE_KEY_CHOOSER, "keyset", ckset, NULL);
    return CRYPTUI_KEY_CHOOSER (obj);
}

GList*
cryptui_key_chooser_get_recipients (CryptUIKeyChooser *chooser)
{
    return cryptui_key_list_get_selected_keys (chooser->priv->keylist);
}

void                
cryptui_key_chooser_set_recipients (CryptUIKeyChooser *chooser, GList *keys)
{
    return cryptui_key_list_set_selected_keys (chooser->priv->keylist, keys);
}

const gchar*        
cryptui_key_chooser_get_signer (CryptUIKeyChooser *chooser)
{
    return cryptui_key_combo_get_key (chooser->priv->keycombo);
}

void                
cryptui_key_chooser_set_signer (CryptUIKeyChooser *chooser, const gchar *key)
{
    return cryptui_key_combo_set_key (chooser->priv->keycombo, key);
}
