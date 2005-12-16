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
    GtkTreeView             *keylist;
    GtkComboBox             *keycombo;
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
construct_recipients (CryptUIKeyChooser *chooser, GtkBox *box)
{
    CryptUIKeyStore *ckstore;
    
    /* TODO: The key search widgets need to be implemented */
    /* TODO: Labels need to be added where appropriate */
    /* TODO: HIG and beautification */
    
    ckstore = cryptui_key_store_new (chooser->priv->ckset, TRUE, NULL);
    cryptui_key_store_set_filter (ckstore, recipients_filter, NULL);
    chooser->priv->keylist = cryptui_key_list_new (ckstore, CRYPTUI_KEY_LIST_CHECKS);
    g_object_unref (ckstore);
    
    gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (chooser->priv->keylist));
    gtk_box_set_child_packing (box, GTK_WIDGET (chooser->priv->keylist), TRUE, 
                               TRUE, 0, GTK_PACK_START);
}

static void
construct_signer (CryptUIKeyChooser *chooser, GtkBox *box)
{
    CryptUIKeyStore *ckstore;
    
    /* TODO: Label needs to be added */
    /* TODO: HIG and beautification */
    
    /* TODO: When only one key is present this should be a checkbox
       ie: 'Sign this Message (as 'key name') */

    /* TODO: i18n */
    ckstore = cryptui_key_store_new (chooser->priv->ckset, TRUE, "None (Don't Sign)");
    cryptui_key_store_set_filter (ckstore, signer_filter, NULL);
    chooser->priv->keycombo = cryptui_key_combo_new (ckstore);
    g_object_unref (ckstore);
    
    gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (chooser->priv->keycombo));
    gtk_box_set_child_packing (box, GTK_WIDGET (chooser->priv->keycombo), FALSE, 
                               TRUE, 0, GTK_PACK_START);
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

    if (chooser->priv->ckset) {
        g_object_unref (chooser->priv->ckset);        
        chooser->priv->ckset = NULL;
    }
    
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
        g_return_if_fail (chooser->priv->ckset == NULL);
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
