/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

/**
 * SeahorseKey: A base class for all keys.
 * 
 * - Keys have a bunch of cached properties that are used to sort, filter, 
 *   find etc... These are stored in the SeahorseKey structure for fast 
 *   access. The derived classes (ie: SeahorsePGPKey) keep these in sync 
 *   whenever the key changes.
 * - Each key keeps note of the SeahorseKeySource it came from.
 *
 * Signals:
 *   destroy: The key was destroyed.
 * 
 * Properties:
 *   key-source: (SeahorseKeySource) The key source this key came from.
 *   key-id: (GQuark) The key identifier in the format ktype:fingerprint 
 *      (ie: the DBUS format)
 *   ktype: (GQuark) The type of key (ie: SKEY_PGP). 
 *   etype: (SeahorseKeyEType) The encryption type (ie: SKEY_PUBLIC)
 *   flags: (guint) Flags on the capabilities of the key (ie: SeahorseKeyFlags)
 *   location: (SeahorseKeyLoc) The location this key is stored. (ie: SKEY_LOC_REMOTE)
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
#include "seahorse-validity.h"

#define SKEY_UNKNOWN                    0

#define SEAHORSE_TYPE_KEY               (seahorse_key_get_type ())
#define SEAHORSE_KEY(obj)               (GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY, SeahorseKey))
#define SEAHORSE_KEY_CLASS(klass)       (GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY, SeahorseKeyClass))
#define SEAHORSE_IS_KEY(obj)            (GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY))
#define SEAHORSE_IS_KEY_CLASS(klass)    (GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY))
#define SEAHORSE_KEY_GET_CLASS(obj)     (GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY, SeahorseKeyClass))

/* 
 * These types should never change. These values are exported via DBUS. In the 
 * case of a key being in multiple locations, the highest location always 'wins'.
 */
typedef enum {
    SKEY_LOC_INVALID =    CRYPTUI_LOC_INVALID,    /* An invalid key */
    SKEY_LOC_UNKNOWN =    CRYPTUI_LOC_MISSING,    /* A key we don't know anything about */
    SKEY_LOC_SEARCHING =  CRYPTUI_LOC_SEARCHING,  /* A key we're searching for but haven't found yet */
    SKEY_LOC_REMOTE =     CRYPTUI_LOC_REMOTE,     /* A key that we've found is present remotely */
    SKEY_LOC_LOCAL =      CRYPTUI_LOC_LOCAL,      /* A key on the local machine */
} SeahorseKeyLoc;

typedef enum {
    SKEY_INFO_NONE,     /* We have no information on this key */
    SKEY_INFO_BASIC,    /* We have the usual basic quick info loaded */
    SKEY_INFO_COMPLETE  /* All info */
} SeahorseKeyInfo;

typedef enum {
    SKEY_ETYPE_NONE =       CRYPTUI_ENCTYPE_NONE,       /* Any encryption type */
    SKEY_SYMMETRIC =        CRYPTUI_ENCTYPE_SYMMETRIC,  /* A symmetric key */
    SKEY_PUBLIC =           CRYPTUI_ENCTYPE_PUBLIC,     /* A public key */
    SKEY_PRIVATE =          CRYPTUI_ENCTYPE_PRIVATE     /* A private key (assumes public/keypair)*/
} SeahorseKeyEType;

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
    SKEY_CHANGE_ALL = 1,
    SKEY_CHANGE_SIGNERS,
    SKEY_CHANGE_PASS,
    SKEY_CHANGE_TRUST,
    SKEY_CHANGE_DISABLED,
    SKEY_CHANGE_EXPIRES,
    SKEY_CHANGE_REVOKERS,
    SKEY_CHANGE_UIDS,
    SKEY_CHANGE_SUBKEYS,
    SKEY_CHANGE_PHOTOS,
    SKEY_CHANGE_PREFERRED
} SeahorseKeyChange;

/* Forward declaration */
struct _SeahorseKeySource;

typedef struct _SeahorseKey SeahorseKey;
typedef struct _SeahorseKeyClass SeahorseKeyClass;

struct _SeahorseKey {
    GtkObject                   parent;

    /*< public >*/
    GQuark                      ktype;
    GQuark                      keyid;
    SeahorseKeyLoc              location;
    SeahorseKeyInfo             loaded;
    SeahorseKeyEType            etype;
    guint                       flags;
    struct _SeahorseKeySource*  sksrc;
    struct _SeahorseKey*        preferred;
};

struct _SeahorseKeyClass {
    GtkObjectClass              parent_class;

    /* signals --------------------------------------------------------- */
    
    /* One of the key's attributes has changed */
    void              (* changed)                 (SeahorseKey        *skey,
                                                   SeahorseKeyChange  change);
    
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

void              seahorse_key_destroy            (SeahorseKey        *skey);

void              seahorse_key_changed            (SeahorseKey        *skey,
                                                   SeahorseKeyChange  change);

GQuark            seahorse_key_get_keyid          (SeahorseKey        *skey);

const gchar*      seahorse_key_get_rawid          (GQuark             keyid);

const gchar*      seahorse_key_get_short_keyid    (SeahorseKey        *skey);

struct _SeahorseKeySource*  
                  seahorse_key_get_source         (SeahorseKey        *skey);

SeahorseKeyEType  seahorse_key_get_etype          (SeahorseKey        *skey);

GQuark            seahorse_key_get_ktype          (SeahorseKey        *skey);

SeahorseKeyInfo   seahorse_key_get_loaded         (SeahorseKey        *skey);

SeahorseKeyLoc    seahorse_key_get_location       (SeahorseKey        *skey);

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

gchar*            seahorse_key_get_simple_name    (SeahorseKey        *skey);

gchar*            seahorse_key_get_fingerprint    (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_validity       (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_trust          (SeahorseKey        *skey);

gulong            seahorse_key_get_expires        (SeahorseKey        *skey);

guint             seahorse_key_get_length         (SeahorseKey        *skey);

const gchar*      seahorse_key_get_stock_id       (SeahorseKey        *skey);

void              seahorse_key_set_preferred      (SeahorseKey        *skey,
                                                   SeahorseKey        *preferred);

gboolean          seahorse_key_lookup_property    (SeahorseKey        *skey, 
                                                   guint              uid, 
                                                   const gchar        *field, 
                                                   GValue             *value);


/* -----------------------------------------------------------------------------
 * KEY PREDICATES
 */
 
typedef gboolean (*SeahorseKeyPredFunc) (SeahorseKey *key, gpointer data);

/* Used for searching, filtering keys */
typedef struct _SeahorseKeyPredicate {
    
    /* Criteria */
    GQuark ktype;                       /* Keys of this type or 0*/
    GQuark keyid;                       /* A specific keyid or 0 */
    SeahorseKeyLoc location;            /* Location of keys or SKEY_LOC_UNKNOWN */
    SeahorseKeyEType etype;             /* Encryption type or SKEY_INVALID */
    guint flags;                        /* Flags keys must have or 0 */
    guint nflags;                       /* Flags keys must not have or 0 */
    struct _SeahorseKeySource *sksrc;   /* key source keys must be from or NULL */

    /* Custom function */
    SeahorseKeyPredFunc custom;
    gpointer custom_data;
    
} SeahorseKeyPredicate;

gboolean 
seahorse_key_predicate_match (SeahorseKeyPredicate *skpred, SeahorseKey *skey);

#endif /* __SEAHORSE_KEY_H__ */
