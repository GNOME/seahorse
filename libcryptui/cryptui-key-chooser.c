/* 
 * Seahorse
 * 
 * Copyright (C) 2005 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "cryptui-key-list.h"
#include "cryptui-key-combo.h"
#include "cryptui-key-chooser.h"
#include "cryptui-priv.h"

enum {
    PROP_0,
    PROP_KEYSET,
    PROP_MODE,
    PROP_ENFORCE_PREFS
};

enum {
    CHANGED,
    LAST_SIGNAL
};


struct _CryptUIKeyChooserPriv {
    guint                   mode;
    guint                   enforce_prefs : 1;
    gboolean                initialized : 1;
    
    CryptUIKeyset           *ckset;
    CryptUIKeyStore         *ckstore;
    GtkTreeView             *keylist;
    GtkComboBox             *keycombo;
    GtkCheckButton          *signercheck;
    
    GtkComboBox             *filtermode;
    GtkEntry                *filtertext;
};

G_DEFINE_TYPE (CryptUIKeyChooser, cryptui_key_chooser, GTK_TYPE_VBOX);
static guint signals[LAST_SIGNAL] = { 0 };

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static gchar* 
get_keyset_value (CryptUIKeyset *keyset, const gchar *key)
{
    gchar *gconf, *value;
    g_return_val_if_fail (keyset, NULL);
    
    gconf = g_strconcat (key, "_", cryptui_keyset_get_keytype (keyset), NULL);
    value = _cryptui_gconf_get_string (gconf);
    g_free (gconf);
    
    return value;
}

static void
set_keyset_value (CryptUIKeyset *keyset, const gchar *key, const gchar *value)
{
    gchar *gconf;
    g_return_if_fail (keyset);
    
    gconf = g_strconcat (key, "_", cryptui_keyset_get_keytype (keyset), NULL);
    _cryptui_gconf_set_string (gconf, value ? value : "");
    g_free (gconf);
}

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
recipients_changed (GtkWidget *widget, CryptUIKeyChooser *chooser)
{
    g_signal_emit (chooser, signals[CHANGED], 0);
}

static void
signer_changed (GtkWidget *widget, CryptUIKeyChooser *chooser)
{
    g_assert (chooser->priv->keycombo);
    
    if (chooser->priv->enforce_prefs) {
        set_keyset_value (cryptui_key_combo_get_keyset (chooser->priv->keycombo), 
                          SEAHORSE_LASTSIGNER_KEY, 
                          cryptui_key_combo_get_key (chooser->priv->keycombo));
    }
    
    g_signal_emit (chooser, signals[CHANGED], 0);
}

static void
signer_toggled (GtkWidget *widget, CryptUIKeyChooser *chooser)
{
    g_assert (chooser->priv->signercheck);
    
    if (chooser->priv->enforce_prefs) {
        set_keyset_value ((CryptUIKeyset *) g_object_get_data ((GObject*) (chooser->priv->signercheck), "ckset"), 
                          SEAHORSE_LASTSIGNER_KEY, 
                          (gchar*) g_object_get_data ((GObject*) (chooser->priv->signercheck), "key"));
    }
    
    g_signal_emit (chooser, signals[CHANGED], 0);
    
}

static void
construct_recipients (CryptUIKeyChooser *chooser, GtkBox *box)
{
    GtkTreeSelection *selection;
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
    cryptui_key_store_set_sortable (chooser->priv->ckstore, TRUE);
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
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->priv->keylist));
    g_signal_connect (selection, "changed", G_CALLBACK (recipients_changed), chooser);
}

static void
construct_signer (CryptUIKeyChooser *chooser, GtkBox *box)
{
    const gchar *none_option = NULL;
    CryptUIKeyStore *ckstore;
    GtkWidget *hbox;
    GtkWidget *label;
    guint count;
    GList *keys;
    gchar *keyname, *labelstr;

    
    /* TODO: HIG and beautification */
        
    if (!(chooser->priv->mode & CRYPTUI_KEY_CHOOSER_MUSTSIGN))
        none_option = _("None (Don't Sign)");

    /* The Sign combo */
    ckstore = cryptui_key_store_new (chooser->priv->ckset, TRUE, none_option);
    cryptui_key_store_set_filter (ckstore, signer_filter, NULL);

    count = cryptui_key_store_get_count (ckstore);
    
    if (count == 1) {
        keys = cryptui_key_store_get_all_keys (ckstore);
        
        keyname = cryptui_keyset_key_display_name (ckstore->ckset, (gchar*) keys->data);
        fprintf (stderr, "Display name is: %s\n", keyname);
        labelstr = g_strdup_printf (_("Sign this message as %s"), keyname);
        fprintf (stderr, "labelstr is: %s\nCreating check button", labelstr);
        
        chooser->priv->signercheck = (GtkCheckButton*) gtk_check_button_new_with_label (labelstr);
        g_object_set_data ((GObject*) (chooser->priv->signercheck), "ckset", ckstore->ckset);
        g_object_set_data ((GObject*) (chooser->priv->signercheck), "key", keys->data);
        
        g_signal_connect (chooser->priv->signercheck , "toggled", G_CALLBACK (signer_toggled), chooser);

        /* Add it in */
        gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (chooser->priv->signercheck));
        gtk_box_set_child_packing (box, GTK_WIDGET (chooser->priv->signercheck), FALSE, TRUE, 0, GTK_PACK_START);

        
        g_free (labelstr);
        g_free (keyname);
        g_list_free (keys);
    } else if (count > 1) {
        /* Top filter box */
        hbox = gtk_hbox_new (FALSE, 12);
        
        /* Sign Label */
        label = gtk_label_new (_("_Sign message as:"));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_container_add (GTK_CONTAINER (hbox), label);
        gtk_box_set_child_packing (GTK_BOX (hbox), label, 
                                   FALSE, TRUE, 0, GTK_PACK_START);
        
        chooser->priv->keycombo = cryptui_key_combo_new (ckstore);
        g_signal_connect (chooser->priv->keycombo, "changed", G_CALLBACK (signer_changed), chooser);
        gtk_container_add (GTK_CONTAINER (hbox), GTK_WIDGET (chooser->priv->keycombo));
        gtk_box_set_child_packing (GTK_BOX (hbox), GTK_WIDGET (chooser->priv->keycombo), 
                                   TRUE, TRUE, 0, GTK_PACK_START);
                                                              
        /* Add it in */
        gtk_container_add (GTK_CONTAINER (box), hbox);
        gtk_box_set_child_packing (box, hbox, FALSE, TRUE, 0, GTK_PACK_START);
    }
    
    g_object_unref (ckstore);
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void
cryptui_key_chooser_init (CryptUIKeyChooser *chooser)
{
    /* init private vars */
    chooser->priv = g_new0 (CryptUIKeyChooserPriv, 1);
    chooser->priv->enforce_prefs = TRUE;
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
    if (chooser->priv->mode & CRYPTUI_KEY_CHOOSER_RECIPIENTS) {
        construct_recipients (chooser, GTK_BOX (obj));
    }
    
    /* The signing area */
    if (chooser->priv->mode & CRYPTUI_KEY_CHOOSER_SIGNER) {
        construct_signer (chooser, GTK_BOX (obj));
        
        if (chooser->priv->enforce_prefs && chooser->priv->keycombo) {
            gchar *id = get_keyset_value (cryptui_key_combo_get_keyset (chooser->priv->keycombo), 
                                          SEAHORSE_LASTSIGNER_KEY);
            cryptui_key_combo_set_key (chooser->priv->keycombo, id);
            g_free (id);
        }
    }

    /* Focus an appropriate widget */
    if (chooser->priv->filtertext)
        gtk_widget_grab_focus (GTK_WIDGET (chooser->priv->filtertext));
    else if (chooser->priv->keylist)
        gtk_widget_grab_focus (GTK_WIDGET (chooser->priv->keylist));
    else if (chooser->priv->keycombo)
        gtk_widget_grab_focus (GTK_WIDGET (chooser->priv->keycombo));
    else if (chooser->priv->signercheck)
        gtk_widget_grab_focus (GTK_WIDGET (chooser->priv->signercheck));
        
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
    
    case PROP_MODE:
        chooser->priv->mode = g_value_get_uint (value);
        break;
    
    case PROP_ENFORCE_PREFS:
        chooser->priv->enforce_prefs = g_value_get_boolean (value);
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
    
    case PROP_MODE:
        g_value_set_uint (value, chooser->priv->mode);
        break;
    
    case PROP_ENFORCE_PREFS:
        g_value_set_boolean (value, chooser->priv->enforce_prefs);
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
    
    g_object_class_install_property (gclass, PROP_MODE,
        g_param_spec_uint ("mode", "Display Mode", "Display mode for chooser",
                           0, 0x0FFFFFFF, 0, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
    
    g_object_class_install_property (gclass, PROP_MODE,
        g_param_spec_boolean ("enforce-prefs", "Enforce User Preferences", "Enforce user preferences",
                              TRUE, G_PARAM_READWRITE));
    
    signals[CHANGED] = g_signal_new ("changed", CRYPTUI_TYPE_KEY_CHOOSER, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (CryptUIKeyChooserClass, changed),
                NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

CryptUIKeyChooser*
cryptui_key_chooser_new (CryptUIKeyset *ckset, CryptUIKeyChooserMode mode)
{
    GObject *obj = g_object_new (CRYPTUI_TYPE_KEY_CHOOSER, "keyset", ckset, 
                                                           "mode", mode, NULL);
    return CRYPTUI_KEY_CHOOSER (obj);
}

gboolean
cryptui_key_chooser_have_recipients (CryptUIKeyChooser *chooser)
{
    g_return_val_if_fail (chooser->priv->keylist != NULL, FALSE);
    return cryptui_key_list_have_selected_keys (chooser->priv->keylist);
}

GList*
cryptui_key_chooser_get_recipients (CryptUIKeyChooser *chooser)
{
    CryptUIKeyset *keyset;
    GList *recipients;
    const gchar *key;
    
    g_return_val_if_fail (chooser->priv->keylist != NULL, NULL);
    recipients = cryptui_key_list_get_selected_keys (chooser->priv->keylist);
    
    if (!chooser->priv->enforce_prefs || 
        !_cryptui_gconf_get_boolean (SEAHORSE_ENCRYPTSELF_KEY))
        return recipients;

    /* If encrypt to self, then add that key */
    keyset = cryptui_key_list_get_keyset (chooser->priv->keylist);
    key = NULL;
    
    /* If we have a signer then use that */
    if (chooser->priv->keycombo)
        key = cryptui_key_combo_get_key (chooser->priv->keycombo);
    
    /* Lookup the default key */
    if (key == NULL) {
        gchar *gval = get_keyset_value (keyset, SEAHORSE_DEFAULT_KEY);
        if (gval != NULL)
            key = _cryptui_keyset_get_internal_keyid (keyset, gval);
    }
    
    /* Use first secret key */
    if (key == NULL) {
        GList *l, *keys = cryptui_keyset_get_keys (keyset);
        for (l = keys; l; l = g_list_next (l)) {
            guint flags = cryptui_keyset_key_flags (keyset, (const gchar*)l->data);
            if (flags & CRYPTUI_FLAG_CAN_SIGN && flags & CRYPTUI_FLAG_CAN_ENCRYPT) {
                key = l->data;
                break;
            }
        }
        g_list_free (keys);
    }

    if (!key)
        g_warning ("Encrypt to self is set, but no personal keys can be found");
    else
        recipients = g_list_prepend (recipients, (gpointer)key);
    
    return recipients;
}

void
cryptui_key_chooser_set_recipients (CryptUIKeyChooser *chooser, GList *keys)
{
    g_return_if_fail (chooser->priv->keylist != NULL);
    cryptui_key_list_set_selected_keys (chooser->priv->keylist, keys);
}

const gchar*
cryptui_key_chooser_get_signer (CryptUIKeyChooser *chooser)
{
    if (chooser->priv->keycombo != NULL)
        return cryptui_key_combo_get_key (chooser->priv->keycombo);
    else if (chooser->priv->signercheck != NULL)
        return gtk_toggle_button_get_active (chooser->priv->signercheck) ? 
                (gchar*) g_object_get_data ((GObject*) (chooser->priv->signercheck), "key"):NULL;
    else
        return NULL;
}

void
cryptui_key_chooser_set_signer (CryptUIKeyChooser *chooser, const gchar *key)
{
    g_return_if_fail (chooser->priv->keycombo != NULL);
    cryptui_key_combo_set_key (chooser->priv->keycombo, key);
}

CryptUIKeyChooserMode
cryptui_key_chooser_get_mode (CryptUIKeyChooser *chooser)
{
    return chooser->priv->mode;
}


