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
 *   key-id: (gchar*) The key identifier (ie: might be a fingerprint).
 *   ktype: (GQuark) The type of key (ie: SKEY_PGP). 
 *   etype: (SeahorseKeyEType) The encryption type (ie: SKEY_PUBLIC)
 *   flags: (guint) Flags on the capabilities of the key (ie: SeahorseKeyFlags)
 *   location: (SeahorseKeyLoc) The location this key is stored. (ie: SKEY_LOC_REMOTE)
 *   loaded: (SeahorseKeyInfo) How much of the key is loaded (ie: SKEY_INFO_COMPLETE)
 *   
 * Properties derived classes must implement:
 *   display-name: (gchar*) The display name for the key.
 *   simple-name: (gchar*) Shortened display name for the key (for use in files etc...).
 *   fingerprint: (gchar*) Displayable fingerprint for the key.
 *   validity: (SeahorseValidity) The key validity.
 *   trust: (SeahorseValidity) Trust for the key.
 *   expires: (gulong) Date this key expires or 0.
 */
 
#ifndef __SEAHORSE_KEY_H__
#define __SEAHORSE_KEY_H__

#include <gtk/gtk.h>

#include "seahorse-validity.h"

#define SKEY_UNKNOWN                    0

#define SEAHORSE_TYPE_KEY               (seahorse_key_get_type ())
#define SEAHORSE_KEY(obj)               (GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_KEY, SeahorseKey))
#define SEAHORSE_KEY_CLASS(klass)       (GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_KEY, SeahorseKeyClass))
#define SEAHORSE_IS_KEY(obj)            (GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_KEY))
#define SEAHORSE_IS_KEY_CLASS(klass)    (GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_KEY))
#define SEAHORSE_KEY_GET_CLASS(obj)     (GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_KEY, SeahorseKeyClass))

typedef enum {
    SKEY_LOC_UNKNOWN,   /* A key we don't know anything about */
    SKEY_LOC_LOCAL,     /* A key on the local machine */
    SKEY_LOC_REMOTE     /* A key that we know is present remotely */
} SeahorseKeyLoc;

typedef enum {
    SKEY_INFO_NONE,     /* We have no information on this key */
    SKEY_INFO_BASIC,    /* We have the usual basic quick info loaded */
    SKEY_INFO_COMPLETE  /* All info */
} SeahorseKeyInfo;

typedef enum {
    SKEY_INVALID,       /* An invalid key */
    SKEY_SYMMETRIC,     /* A symmetric key */
    SKEY_PUBLIC,        /* A public key */
    SKEY_PRIVATE        /* A private key (assumes public/keypair)*/
} SeahorseKeyEType;

typedef enum {
    SKEY_FLAG_IS_VALID = 0x0001,
    SKEY_FLAG_CAN_ENCRYPT = 0x0002,
    SKEY_FLAG_CAN_SIGN = 0x0004,
    SKEY_FLAG_EXPIRED = 0x0010,
    SKEY_FLAG_REVOKED = 0x0020,
    SKEY_FLAG_DISABLED = 0x0040,
    SKEY_FLAG_TRUSTED = 0x0100
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
    SKEY_CHANGE_PHOTOS
} SeahorseKeyChange;

/* Forward declaration */
struct _SeahorseKeySource;

typedef struct _SeahorseKey SeahorseKey;
typedef struct _SeahorseKeyClass SeahorseKeyClass;
	
struct _SeahorseKey {
    GtkObject		            parent;
	
    /*< public >*/
    GQuark                      ktype;
    const gchar*                keyid;
    SeahorseKeyLoc              location;
    SeahorseKeyInfo             loaded;
    SeahorseKeyEType            etype;
    guint                       flags;
    struct _SeahorseKeySource*  sksrc;
};

struct _SeahorseKeyClass {
	GtkObjectClass              parent_class;

    /* signals --------------------------------------------------------- */
    	
	/* One of the key's attributes has changed */
	void 			(* changed)		              (SeahorseKey        *skey,
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
};

GType             seahorse_key_get_type (void);

void              seahorse_key_destroy            (SeahorseKey        *skey);

void              seahorse_key_changed            (SeahorseKey        *skey,
                                                   SeahorseKeyChange  change);

const gchar*      seahorse_key_get_keyid          (SeahorseKey        *skey);

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

gchar*            seahorse_key_get_display_name   (SeahorseKey        *skey);

gchar*            seahorse_key_get_simple_name    (SeahorseKey        *skey);

gchar*            seahorse_key_get_fingerprint    (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_validity       (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_trust          (SeahorseKey        *skey);

gulong            seahorse_key_get_expires        (SeahorseKey        *skey);

/* -----------------------------------------------------------------------------
 * KEY PREDICATES
 */
 
typedef gboolean (*SeahorseKeyPredFunc) (SeahorseKey *key, gpointer data);

/* Used for searching, filtering keys */
typedef struct _SeahorseKeyPredicate {
    
    /* Criteria */
    GQuark ktype;
    const gchar *keyid;
    SeahorseKeyLoc location;
    SeahorseKeyEType etype;
    guint flags;
    struct _SeahorseKeySource *sksrc;

    /* Custom function */
    SeahorseKeyPredFunc custom;
    gpointer custom_data;
    
} SeahorseKeyPredicate;

gboolean 
seahorse_key_predicate_match (SeahorseKeyPredicate *skpred, SeahorseKey *skey);

#endif /* __SEAHORSE_KEY_H__ */
