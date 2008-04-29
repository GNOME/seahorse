/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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

#include "seahorse-context.h"
#include "seahorse-unknown-source.h"
#include "seahorse-unknown-key.h"

#include "common/sea-registry.h"

enum {
    PROP_0,
    PROP_KEY_TYPE,
    PROP_KEY_DESC,
    PROP_LOCATION
};

G_DEFINE_TYPE (SeahorseUnknownSource, seahorse_unknown_source, SEAHORSE_TYPE_KEY_SOURCE);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static void
search_done (SeahorseOperation *op, SeahorseKey *skey)
{
    skey->location = SKEY_LOC_UNKNOWN;
}

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static SeahorseOperation*
seahorse_unknown_source_load (SeahorseKeySource *src, GQuark keyid)
{
    SeahorseUnknownSource *usrc;
    
    g_assert (SEAHORSE_IS_UNKNOWN_SOURCE (src));
    usrc = SEAHORSE_UNKNOWN_SOURCE (src);
    
    if (keyid) {
        g_return_val_if_fail (keyid, NULL);
        seahorse_unknown_source_add_key (usrc, keyid, NULL);
    }
    
    return seahorse_operation_new_complete (NULL);
}

static void
seahorse_unknown_source_stop (SeahorseKeySource *src)
{

}

static guint
seahorse_unknown_source_get_state (SeahorseKeySource *src)
{
    return 0;
}

static SeahorseOperation* 
seahorse_unknown_source_import (SeahorseKeySource *sksrc, GInputStream *input)
{
    g_return_val_if_reached (NULL);
    return NULL;
}

static SeahorseOperation* 
seahorse_unknown_source_export_raw (SeahorseKeySource *sksrc, GSList *keyids, 
                                    GOutputStream *output)
{
    g_return_val_if_reached (NULL);
    return NULL;
}

static gboolean            
seahorse_unknown_source_remove (SeahorseKeySource *sksrc, SeahorseKey *skey,
                                guint name, GError **error)
{
    g_return_val_if_fail (sksrc == seahorse_key_get_source (skey), FALSE);
    seahorse_key_destroy (skey);
    return TRUE;
}

static void 
seahorse_unknown_source_set_property (GObject *object, guint prop_id, const GValue *value, 
                                      GParamSpec *pspec)
{
    SeahorseUnknownSource *usrc = SEAHORSE_UNKNOWN_SOURCE (object);
    
    switch (prop_id) {
    case PROP_KEY_TYPE:
        usrc->ktype = g_value_get_uint (value);
        break;
    }
}

static void 
seahorse_unknown_source_get_property (GObject *object, guint prop_id, GValue *value, 
                                  GParamSpec *pspec)
{
    SeahorseUnknownSource *usrc = SEAHORSE_UNKNOWN_SOURCE (object);
    
    switch (prop_id) {
    case PROP_KEY_TYPE:
        g_value_set_uint (value, usrc->ktype);
        break;
    case PROP_KEY_DESC:
        g_value_set_string (value, _("Unavailable Key"));
        break;
    case PROP_LOCATION:
        g_value_set_uint (value, SKEY_LOC_UNKNOWN);
        break;
    }
}

static void
seahorse_unknown_source_init (SeahorseUnknownSource *ssrc)
{

}

static void
seahorse_unknown_source_class_init (SeahorseUnknownSourceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    SeahorseKeySourceClass *parent_class = SEAHORSE_KEY_SOURCE_CLASS (klass);
   
    seahorse_unknown_source_parent_class = g_type_class_peek_parent (klass);
    
    gobject_class->set_property = seahorse_unknown_source_set_property;
    gobject_class->get_property = seahorse_unknown_source_get_property;
    
    parent_class->load = seahorse_unknown_source_load;
    parent_class->stop = seahorse_unknown_source_stop;
    parent_class->get_state = seahorse_unknown_source_get_state;
    parent_class->import = seahorse_unknown_source_import;
    parent_class->export_raw = seahorse_unknown_source_export_raw;
    parent_class->remove = seahorse_unknown_source_remove;
 
    g_object_class_install_property (gobject_class, PROP_KEY_TYPE,
        g_param_spec_uint ("key-type", "Key Type", "Key type that originates from this key source.", 
                           0, G_MAXUINT, SKEY_UNKNOWN, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (gobject_class, PROP_KEY_DESC,
        g_param_spec_string ("key-desc", "Key Desc", "Description for keys that originate here.",
                             NULL, G_PARAM_READABLE));

    g_object_class_install_property (gobject_class, PROP_LOCATION,
        g_param_spec_uint ("location", "Key Location", "Where the key is stored. See SeahorseKeyLoc", 
                           0, G_MAXUINT, SKEY_LOC_INVALID, G_PARAM_READABLE));    
    
	sea_registry_register_type (NULL, SEAHORSE_TYPE_UNKNOWN_SOURCE, "key-source", NULL);
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

SeahorseUnknownSource*
seahorse_unknown_source_new (GQuark ktype)
{
   return g_object_new (SEAHORSE_TYPE_UNKNOWN_SOURCE, "key-type", ktype, NULL);
}

SeahorseKey*                     
seahorse_unknown_source_add_key (SeahorseUnknownSource *usrc, GQuark keyid,
                                 SeahorseOperation *search)
{
    SeahorseKey *skey;

    g_return_val_if_fail (keyid != 0, NULL);

    skey = seahorse_context_get_key (SCTX_APP (), SEAHORSE_KEY_SOURCE (usrc), keyid);
    if (!skey) {
        skey = SEAHORSE_KEY (seahorse_unknown_key_new (usrc, keyid));
        seahorse_context_add_key (SCTX_APP (), skey);
    }
    
    if (search) {
        skey->location = SKEY_LOC_SEARCHING;
        seahorse_operation_watch (search, G_CALLBACK (search_done), NULL, skey);
    }
    
    return skey;
}
