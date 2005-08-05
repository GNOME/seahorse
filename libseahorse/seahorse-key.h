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

#ifndef __SEAHORSE_KEY_H__
#define __SEAHORSE_KEY_H__

#include <gtk/gtk.h>

#include "seahorse-validity.h"

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
} SeahorseKeyType;

typedef enum {
    SKEY_FLAG_IS_VALID = 0x0001,
    SKEY_FLAG_CAN_ENCRYPT = 0x0002,
    SKEY_FLAG_CAN_SIGN = 0x0004,
    SKEY_FLAG_EXPIRED = 0x0010,
    SKEY_FLAG_REVOKED = 0x0020
} SeahorseKeyFlags;

/* Possible key changes */
typedef enum {
    SKEY_CHANGE_ALL,
	SKEY_CHANGE_SIGNERS,
	SKEY_CHANGE_PASS,
	SKEY_CHANGE_TRUST,
	SKEY_CHANGE_DISABLED,
	SKEY_CHANGE_EXPIRES,
	SKEY_CHANGE_REVOKERS,
	SKEY_CHANGE_UIDS,
	SKEY_CHANGE_SUBKEYS
} SeahorseKeyChange;

/* Forward declaration */
struct _SeahorseKeySource;

typedef struct _SeahorseKey SeahorseKey;
typedef struct _SeahorseKeyClass SeahorseKeyClass;
	
struct _SeahorseKey {
    GtkObject		            parent;
	
    /*< public >*/
    const gchar*                keyid;
    SeahorseKeyLoc              location;
    SeahorseKeyInfo             loaded;
    SeahorseKeyType             type;
    guint                       flags;
    struct _SeahorseKeySource*  key_source;

    /*< private >*/
    gpointer                    priv;    
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
    
    /* Get the display name for the key or UID */
    gchar*            (*get_name)                 (SeahorseKey  *skey,
                                                   guint        uid);

    /* properties ------------------------------------------------------ 
     *
     * key-source: The key source
     * key-id: The key id
     * key-type: The key type (public, private etc...)
     * flags: The key flags
     * location: The key location
     * loaded: What part of the key is loaded
     * 
     * display-name: The display name
     * simple-name: Simple name for the key (for use in files etc...)
     * fingerprint: Fingerprint for the key 
     * validity: The key validity 
     * trust: Trust for the key 
     * expired: Expired key
     * expires: Date expires or 0
     */
};

GType             seahorse_key_get_type (void);

void              seahorse_key_destroy            (SeahorseKey        *skey);

void              seahorse_key_changed            (SeahorseKey        *skey,
                                                   SeahorseKeyChange  change);

const gchar*      seahorse_key_get_keyid          (SeahorseKey        *skey);

struct _SeahorseKeySource*  
                  seahorse_key_get_source         (SeahorseKey        *skey);

SeahorseKeyType   seahorse_key_get_keytype        (SeahorseKey        *skey);

SeahorseKeyInfo   seahorse_key_get_loaded         (SeahorseKey        *skey);

SeahorseKeyLoc    seahorse_key_get_location       (SeahorseKey        *skey);

guint             seahorse_key_get_flags          (SeahorseKey        *skey);

guint             seahorse_key_get_num_names      (SeahorseKey        *skey);

gchar*            seahorse_key_get_name           (SeahorseKey        *skey,
                                                   guint              uid);

gchar*            seahorse_key_get_display_name   (SeahorseKey        *skey);

gchar*            seahorse_key_get_simple_name    (SeahorseKey        *skey);

gchar*            seahorse_key_get_fingerprint    (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_validity       (SeahorseKey        *skey);

SeahorseValidity  seahorse_key_get_trust          (SeahorseKey        *skey);

gulong            seahorse_key_get_expires        (SeahorseKey        *skey);

#endif /* __SEAHORSE_KEY_H__ */
