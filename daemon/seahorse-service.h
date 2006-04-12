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

#ifndef __SEAHORSE_SERVICE_H__
#define __SEAHORSE_SERVICE_H__

#include <glib/gerror.h>
#include <glib-object.h>

#include "seahorse-keyset.h"

G_BEGIN_DECLS

/* TODO: This needs to be refined */
enum {
    SEAHORSE_DBUS_ERROR_INVALID = 1,
    SEAHORSE_DBUS_ERROR_CRYPTO = 2,
    SEAHORSE_DBUS_ERROR_NOTIMPLEMENTED = 100
};

#define SEAHORSE_DBUS_ERROR  g_quark_from_static_string ("seahorse")

typedef struct _SeahorseService SeahorseService;
typedef struct _SeahorseServiceClass SeahorseServiceClass;
    
#define SEAHORSE_TYPE_SERVICE               (seahorse_service_get_type ())
#define SEAHORSE_SERVICE(object)            (G_TYPE_CHECK_INSTANCE_CAST((object), SEAHORSE_TYPE_SERVICE, SeahorseService))
#define SEAHORSE_SERVICE_CLASS(klass)       (G_TYPE_CHACK_CLASS_CAST((klass), SEAHORSE_TYPE_SERVICE, SeahorseServiceClass))
#define SEAHORSE_IS_SERVICE(object)         (G_TYPE_CHECK_INSTANCE_TYPE((object), SEAHORSE_TYPE_SERVICE))
#define SEAHORSE_IS_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass), SEAHORSE_TYPE_SERVICE))
#define SEAHORSE_SERVICE_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS((object), SEAHORSE_TYPE_SERVICE, SeahorseServiceClass))

struct _SeahorseService {
	GObject base;
    
    /* <public> */
    GHashTable *keysets;
};

struct _SeahorseServiceClass {
	GObjectClass base;
};

GType          seahorse_service_get_type            (void);

gboolean       seahorse_service_get_key_types       (SeahorseService *svc, gchar*** ret, 
                                                     GError **error);


gboolean        seahorse_service_get_keyset         (SeahorseService *svc, gchar *ktype, 
                                                     gchar **path, GError **error);

gboolean        seahorse_service_has_key_field      (SeahorseService *svc, gchar *key, 
                                                     gchar *field, gboolean *has, GError **error);

gboolean        seahorse_service_get_key_field      (SeahorseService *svc, gchar *key, 
                                                     gchar *field, GValue *value, GError **error);

gboolean        seahorse_service_get_key_fields     (SeahorseService *svc, gchar *key, 
                                                     gchar **fields, GHashTable **values, GError **error);

gboolean        seahorse_service_get_key_names      (SeahorseService *svc, gchar *key, 
                                                     gchar ***names, gchar ***cns, GError **error);

gboolean        seahorse_service_import_keys        (SeahorseService *svc, gchar *ktype, 
                                                     gchar *data, gchar ***keys, GError **error);
                              
gboolean        seahorse_service_export_keys        (SeahorseService *svc, gchar *ktype,
                                                     gchar **keys, gchar **data, GError **error);

gboolean        seahorse_service_match_keys         (SeahorseService *svc, gchar *ktype, 
                                                     gint flags, gchar **patterns, gchar ***keys, 
                                                     gchar***unmatched, GError **error);

gboolean        seahorse_service_match_save         (SeahorseService *svc, gchar *ktype, 
                                                     gint flags, gchar **patterns, 
                                                     gchar **keys, GError **error);

gboolean        seahorse_service_discover_keys      (SeahorseService *svc, gchar *ktype, 
                                                     gint flags, gchar **patterns, gchar **keys, 
                                                     GError **error);

SeahorseKey*    seahorse_service_key_from_dbus      (const gchar *key, guint *uid);

gchar*          seahorse_service_key_to_dbus        (SeahorseKey *skey, guint uid);

gchar*          seahorse_service_keyid_to_dbus      (GQuark ktype, const gchar *keyid, guint uid);

/* -----------------------------------------------------------------------------
 * KEYSET SERVICE
 */
 
typedef struct _SeahorseServiceKeyset SeahorseServiceKeyset;
typedef struct _SeahorseServiceKeysetClass SeahorseServiceKeysetClass;
    
#define SEAHORSE_TYPE_SERVICE_KEYSET               (seahorse_service_keyset_get_type ())
#define SEAHORSE_SERVICE_KEYSET(object)            (G_TYPE_CHECK_INSTANCE_CAST((object), SEAHORSE_TYPE_SERVICE_KEYSET, SeahorseServiceKeyset))
#define SEAHORSE_SERVICE_KEYSET_CLASS(klass)       (G_TYPE_CHACK_CLASS_CAST((klass), SEAHORSE_TYPE_SERVICE_KEYSET, SeahorseServiceKeysetClass))
#define SEAHORSE_IS_SERVICE_KEYSET(object)         (G_TYPE_CHECK_INSTANCE_TYPE((object), SEAHORSE_TYPE_SERVICE_KEYSET))
#define SEAHORSE_IS_SERVICE_KEYSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass), SEAHORSE_TYPE_SERVICE_KEYSET))
#define SEAHORSE_SERVICE_KEYSET_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS((object), SEAHORSE_TYPE_SERVICE_KEYSET, SeahorseServiceKeysetClass))

struct _SeahorseServiceKeyset {
    SeahorseKeyset base;
};

struct _SeahorseServiceKeysetClass {
    SeahorseKeysetClass base;
    
    /* signals --------------------------------------------------------- */
    
    /* A key was added to this view */
    void (*key_added)   (SeahorseServiceKeyset *skset, const gchar *key);

    /* Removed a key from this view */
    void (*key_removed) (SeahorseServiceKeyset *skset, const gchar *key);
    
    /* One of the key's attributes has changed */
    void (*key_changed) (SeahorseServiceKeyset *skset, const gchar *key);   
};

GType           seahorse_service_keyset_get_type       (void);

SeahorseKeyset* seahorse_service_keyset_new            (GQuark keytype);

gboolean        seahorse_service_keyset_list_keys      (SeahorseServiceKeyset *keyset,
                                                        gchar ***keys, GError **error);

/* -----------------------------------------------------------------------------
 * CRYPTO SERVICE
 */

typedef struct _SeahorseServiceCrypto SeahorseServiceCrypto;
typedef struct _SeahorseServiceCryptoClass SeahorseServiceCryptoClass;
    
#define SEAHORSE_TYPE_SERVICE_CRYPTO               (seahorse_service_crypto_get_type ())
#define SEAHORSE_SERVICE_CRYPTO(object)            (G_TYPE_CHECK_INSTANCE_CAST((object), SEAHORSE_TYPE_SERVICE_CRYPTO, SeahorseServiceCrypto))
#define SEAHORSE_SERVICE_CRYPTO_CLASS(klass)       (G_TYPE_CHACK_CLASS_CAST((klass), SEAHORSE_TYPE_SERVICE_CRYPTO, SeahorseServiceCryptoClass))
#define SEAHORSE_IS_SERVICE_CRYPTO(object)         (G_TYPE_CHECK_INSTANCE_TYPE((object), SEAHORSE_TYPE_SERVICE_CRYPTO))
#define SEAHORSE_IS_SERVICE_CRYPTO_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass), SEAHORSE_TYPE_SERVICE_CRYPTO))
#define SEAHORSE_SERVICE_CRYPTO_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS((object), SEAHORSE_TYPE_SERVICE_CRYPTO, SeahorseServiceCryptoClass))

struct _SeahorseServiceCrypto {
    GObject base;
};

struct _SeahorseServiceCryptoClass {
    GObjectClass base;
};

GType                   seahorse_service_crypto_get_type       (void);

SeahorseServiceCrypto*  seahorse_service_crypto_new            ();

gboolean                seahorse_service_crypto_encrypt_text    (SeahorseServiceCrypto *crypto, 
                                                                 const char **recipients, 
                                                                 const char *signer, int flags, 
                                                                 const char *cleartext, 
                                                                 char **crypttext, GError **error);

gboolean                seahorse_service_crypto_sign_text       (SeahorseServiceCrypto *crypto, 
                                                                 const char *signer, int flags, 
                                                                 const char *cleartext, 
                                                                 char **crypttext, GError **error);

gboolean                seahorse_service_crypto_decrypt_text    (SeahorseServiceCrypto *crypto, 
                                                                 int flags, const char *crypttext, 
                                                                 char **cleartext, char **signer,
                                                                 GError **error);

gboolean                seahorse_service_crypto_verify_text     (SeahorseServiceCrypto *crypto, 
                                                                 int flags, const char *crypttext, 
                                                                 char **cleartext, char **signer,
                                                                 GError **error);

G_END_DECLS

#endif /* __SEAHORSE_SERVICE_H__ */
