/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

/**
 * SeahorseKey: A base class for all keys.
 * 
 * - Keys have a bunch of cached properties that are used to sort, filter, 
 *   find etc... These are stored in the SeahorseKey structure for fast 
 *   access. The derived classes (ie: SeahorsePGPKey) keep these in sync 
 *   whenever the key changes.
 * - Each key keeps note of the SeahorseSource it came from.
 *
 * Signals:
 *   destroy: The key was destroyed.
 * 
 * Properties:
 *   key-source: (SeahorseSource) The key source this key came from.
 *   key-id: (GQuark) The key identifier in the format ktype:fingerprint 
 *      (ie: the DBUS format)
 *   raw-id: (gchar*) The raw backend specific key identifier
 *   key-desc: (gchar*) A description of the key type.
 *   ktype: (GQuark) The type of key (ie: SEAHORSE_PGP). 
 *   etype: (SeahorseUsage) The encryption type (ie: SEAHORSE_USAGE_PUBLIC_KEY)
 *   flags: (guint) Flags on the capabilities of the key (ie: SeahorseKeyFlags)
 *   location: (SeahorseLocation) The location this key is stored. (ie: SEAHORSE_LOCATION_REMOTE)
 *   loaded: (SeahorseKeyInfo) How much of the key is loaded (ie: SKEY_INFO_COMPLETE)
 *   preferred: (SeahorseKey) Another representation of this key, that is better suited
 *      for use. (eg: for a remote key could point to it's local counterpart)
 *   
 * Properties derived classes must implement:
 *   display-name: (gchar*) The display name for the key.
 *   display-id: (gchar*) The Key ID to display
 *   simple-name: (gchar*) Shortened display name for the key (for use in files etc...).
 *   fingerprint: (gchar*) Displayable fingerprint for the key.
 *   validity: (SeahorseValidity) The key validity.
 *   trust: (SeahorseValidity) Trust for the key.
 *   expires: (gulong) Date this key expires or 0.
 *   length: (guint) The length of the key in bits.
 *   stock-id: (gpointer/string) The icon stock id
 */
 
#ifndef __SEAHORSE_KEY_H__
#define __SEAHORSE_KEY_H__

#include <gtk/gtk.h>

#include "cryptui.h"
#include "seahorse-object.h"
#include "seahorse-validity.h"

#define SKEY_UNKNOWN                    0

#define SEAHORSE_TYPE_KEY               (seahorse_key_get_type ())
#define SEAHORSE_KEY(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_KEY, SeahorseKey))
#define SEAHORSE_KEY_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY, SeahorseKeyClass))
#define SEAHORSE_IS_KEY(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_KEY))
#define SEAHORSE_IS_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY))
#define SEAHORSE_KEY_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_KEY, SeahorseKeyClass))

typedef enum {
    SKEY_INFO_NONE,     /* We have no information on this key */
    SKEY_INFO_BASIC,    /* We have the usual basic quick info loaded */
    SKEY_INFO_COMPLETE  /* All info */
} SeahorseKeyInfo;

typedef enum {
    SKEY_FLAG_IS_VALID =    CRYPTUI_FLAG_IS_VALID,
    SKEY_FLAG_CAN_ENCRYPT = CRYPTUI_FLAG_CAN_ENCRYPT,
    SKEY_FLAG_CAN_SIGN =    CRYPTUI_FLAG_CAN_SIGN,
    SKEY_FLAG_EXPIRED =     CRYPTUI_FLAG_EXPIRED,
    SKEY_FLAG_REVOKED =     CRYPTUI_FLAG_REVOKED,
    SKEY_FLAG_DISABLED =    CRYPTUI_FLAG_DISABLED,
    SKEY_FLAG_TRUSTED =     CRYPTUI_FLAG_TRUSTED
} SeahorseKeyFlags;

/* Possible key changes */
typedef enum {
    SKEY_CHANGE_ALL = SEAHORSE_OBJECT_CHANGE_ALL,
    SKEY_CHANGE_SIGNERS = SEAHORSE_OBJECT_CHANGE_MAX,
    SKEY_CHANGE_PASS,
    SKEY_CHANGE_TRUST,
    SKEY_CHANGE_DISABLED,
    SKEY_CHANGE_EXPIRES,
    SKEY_CHANGE_REVOKERS,
    SKEY_CHANGE_UIDS,
    SKEY_CHANGE_SUBKEYS,
    SKEY_CHANGE_PHOTOS
} SeahorseKeyChange;

/* Forward declaration */
struct _SeahorseSource;
struct _SeahorseContext;

typedef struct _SeahorseKey SeahorseKey;
typedef struct _SeahorseKeyClass SeahorseKeyClass;

struct _SeahorseKey {
    SeahorseObject              parent;

    /*< public >*/
    const gchar*                keydesc;
    const gchar*                rawid;
    SeahorseKeyInfo             loaded;
};

struct _SeahorseKeyClass {
    SeahorseObjectClass         parent_class;

    /* virtual methods ------------------------------------------------- */

    /* The number of UIDs on the key */
    guint             (*get_num_names)            (SeahorseKey  *skey);
    
    /* Get the display name for the specified UID */
    gchar*            (*get_name)                 (SeahorseKey  *skey,
                                                   guint        uid);
    
    /* Get the CN for the specified UID */
    gchar*            (*get_name_cn)              (SeahorseKey  *skey,
                                                   guint        uid);
    
    /* Get the markup for the specified UID */
    gchar*            (*get_name_markup)          (SeahorseKey *skey,
                                                   guint       uid);
    
    /* Get the validity for the specified UID */
    SeahorseValidity  (*get_name_validity)        (SeahorseKey  *skey,
                                                   guint        uid);
};

GType             seahorse_key_get_type (void);

GQuark            seahorse_key_get_keyid          (SeahorseKey        *skey);

const gchar*      seahorse_key_get_rawid          (GQuark             keyid);

const gchar*      seahorse_key_get_short_keyid    (SeahorseKey        *skey);

struct _SeahorseSource*  
                  seahorse_key_get_source         (SeahorseKey        *skey);

SeahorseUsage     seahorse_key_get_usage          (SeahorseKey        *skey);

GQuark            seahorse_key_get_ktype          (SeahorseKey        *skey);

const gchar*      seahorse_key_get_desc           (SeahorseKey        *skey);

SeahorseKeyInfo   seahorse_key_get_loaded         (SeahorseKey        *skey);

SeahorseLocation  seahorse_key_get_location       (SeahorseKey        *skey);

guint             seahorse_key_get_flags          (SeahorseKey        *skey);

guint             seahorse_key_get_num_names      (SeahorseKey        *skey);

gchar*            seahorse_key_get_name           (SeahorseKey        *skey,
                                                   guint              name);

gchar*            seahorse_key_get_name_cn        (SeahorseKey        *skey,
                                                   guint              name);

gchar*            seahorse_key_get_name_markup    (SeahorseKey        *skey,
                                                   guint              name);
                                                   
SeahorseValidity  seahorse_key_get_name_validity  (SeahorseKey        *skey,
                                                   guint              uid);

gchar*            seahorse_key_get_display_name   (SeahorseKey        *skey);

gchar*            seahorse_key_get_display_id     (SeahorseKey        *skey);

gchar*            seahorse_key_get_simple_name    (SeahorseKey        *skey);

gchar*            seahorse_key_get_fingerprint    (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_validity       (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_trust          (SeahorseKey        *skey);

gulong            seahorse_key_get_expires        (SeahorseKey        *skey);

guint             seahorse_key_get_length         (SeahorseKey        *skey);

const gchar*      seahorse_key_get_stock_id       (SeahorseKey        *skey);

gboolean          seahorse_key_lookup_property    (SeahorseKey        *skey, 
                                                   guint              uid, 
                                                   const gchar        *field, 
                                                   GValue             *value);

#endif /* __SEAHORSE_KEY_H__ */
