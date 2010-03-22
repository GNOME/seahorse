/*
 * Seahorse
 *
 * Copyright (C) 2004 Stefan Walter
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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>

#include "seahorse-operation.h"
#include "seahorse-ldap-source.h"
#include "seahorse-hkp-source.h"
#include "seahorse-server-source.h"
#include "seahorse-util.h"
#include "seahorse-pgp-key.h"

#include "common/seahorse-registry.h"

/**
 * SECTION:seahorse-server-source
 * @short_description: Objects for handling of internet sources of keys (hkp/ldap)
 * @include:seahorse-server-source.h
 *
 **/

enum {
    PROP_0,
    PROP_SOURCE_TAG,
    PROP_SOURCE_LOCATION,
    PROP_KEY_SERVER,
    PROP_URI
};

/* -----------------------------------------------------------------------------
 *  SERVER SOURCE
 */
 
struct _SeahorseServerSourcePrivate {
    SeahorseMultiOperation *mop;
    gchar *server;
    gchar *uri;
};

static void seahorse_source_iface (SeahorseSourceIface *iface);

G_DEFINE_TYPE_EXTENDED (SeahorseServerSource, seahorse_server_source, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (SEAHORSE_TYPE_SOURCE, seahorse_source_iface));

/* GObject handlers */
static void seahorse_server_source_dispose    (GObject *gobject);
static void seahorse_server_source_finalize   (GObject *gobject);
static void seahorse_server_get_property      (GObject *object, guint prop_id,
                                               GValue *value, GParamSpec *pspec);
static void seahorse_server_set_property      (GObject *object, guint prop_id,
                                               const GValue *value, GParamSpec *pspec);
                                               
/* SeahorseSource methods */
static SeahorseOperation*  seahorse_server_source_load (SeahorseSource *src);

static GObjectClass *parent_class = NULL;


/**
* klass: Class to initialize
*
* Initialize the basic class stuff
*
**/
static void
seahorse_server_source_class_init (SeahorseServerSourceClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = seahorse_server_source_dispose;
    gobject_class->finalize = seahorse_server_source_finalize;
    gobject_class->set_property = seahorse_server_set_property;
    gobject_class->get_property = seahorse_server_get_property;

	g_object_class_override_property (gobject_class, PROP_SOURCE_TAG, "source-tag");
	g_object_class_override_property (gobject_class, PROP_SOURCE_LOCATION, "source-location");
	
    g_object_class_install_property (gobject_class, PROP_KEY_SERVER,
            g_param_spec_string ("key-server", "Key Server",
                                 "Key Server to search on", "",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, PROP_URI,
            g_param_spec_string ("uri", "Key Server URI",
                                 "Key Server full URI", "",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
    
	seahorse_registry_register_function (NULL, seahorse_pgp_key_canonize_id, "canonize", SEAHORSE_PGP_STR, NULL);
}

/**
* iface: The #SeahorseSourceIface to init
*
* Sets the load function in @iface
*
* This is the init function of the interface SEAHORSE_TYPE_SOURCE
*
**/
static void 
seahorse_source_iface (SeahorseSourceIface *iface)
{
	iface->load = seahorse_server_source_load;
}

/**
* ssrc: A #SeahorseServerSource object
*
* init context, private vars, set prefs, connect signals
*
**/
static void
seahorse_server_source_init (SeahorseServerSource *ssrc)

{
    /* init private vars */
    ssrc->priv = g_new0 (SeahorseServerSourcePrivate, 1);
    ssrc->priv->mop = seahorse_multi_operation_new ();
}


/**
* gobject: A #SeahorseServerSource object
*
* dispose of all our internal references
*
**/
static void
seahorse_server_source_dispose (GObject *gobject)
{
    SeahorseServerSource *ssrc;
    SeahorseSource *sksrc;
    
    /*
     * Note that after this executes the rest of the object should
     * still work without a segfault. This basically nullifies the 
     * object, but doesn't free it.
     * 
     * This function should also be able to run multiple times.
     */
  
    ssrc = SEAHORSE_SERVER_SOURCE (gobject);
    sksrc = SEAHORSE_SOURCE (gobject);
    g_assert (ssrc->priv);
    
    /* Clear out all the operations */
    if (ssrc->priv->mop) {
        if(seahorse_operation_is_running (SEAHORSE_OPERATION (ssrc->priv->mop)))
            seahorse_operation_cancel (SEAHORSE_OPERATION (ssrc->priv->mop));
        g_object_unref (ssrc->priv->mop);
        ssrc->priv->mop = NULL;
    }
 
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}


/**
* gobject: A #SeahorseServerSource object
*
* free private vars
*
**/
static void
seahorse_server_source_finalize (GObject *gobject)
{
    SeahorseServerSource *ssrc;
  
    ssrc = SEAHORSE_SERVER_SOURCE (gobject);
    g_assert (ssrc->priv);
    
    g_free (ssrc->priv->server);
    g_free (ssrc->priv->uri);
    g_assert (ssrc->priv->mop == NULL);    
    g_free (ssrc->priv);
 
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/**
* object: A SeahorseServerSource object
* prop_id: The ID of the property to set
* value: The value to set
* pspec: ignored
*
* Properties that can be set:
* PROP_KEY_SERVER, PROP_URI
*
**/
static void 
seahorse_server_set_property (GObject *object, guint prop_id, 
                              const GValue *value, GParamSpec *pspec)
{
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (object);
 
    switch (prop_id) {
    case PROP_KEY_SERVER:
        g_assert (ssrc->priv->server == NULL);
        ssrc->priv->server = g_strdup (g_value_get_string (value));
        g_return_if_fail (ssrc->priv->server && ssrc->priv->server[0]);
        break;
    case PROP_URI:
        g_assert (ssrc->priv->uri == NULL);
        ssrc->priv->uri = g_strdup (g_value_get_string (value));
        g_return_if_fail (ssrc->priv->uri && ssrc->priv->uri[0]);
        break;
    default:
        break;
    }  
}

/**
* object: A #SeahorseServerSource object
* prop_id: The id of the property
* value: The value to get
* pspec: ignored
*
* The properties that can be read are:
* PROP_KEY_SERVER, PROP_URI, PROP_SOURCE_TAG, PROP_SOURCE_LOCATION
*
**/
static void 
seahorse_server_get_property (GObject *object, guint prop_id, GValue *value,
                              GParamSpec *pspec)
{
    SeahorseServerSource *ssrc = SEAHORSE_SERVER_SOURCE (object);
  
    switch (prop_id) {
    case PROP_KEY_SERVER:
        g_value_set_string (value, ssrc->priv->server);
        break;
    case PROP_URI:
        g_value_set_string (value, ssrc->priv->uri);
        break;
    case PROP_SOURCE_TAG:
        g_value_set_uint (value, SEAHORSE_PGP);
        break;
    case PROP_SOURCE_LOCATION:
        g_value_set_enum (value, SEAHORSE_LOCATION_REMOTE);
        break;
    }        
}


/* --------------------------------------------------------------------------
 * HELPERS 
 */

/**
 * seahorse_server_source_take_operation:
 * @ssrc: the #SeahorseServerSource to add the @op to
 * @op: add this to the multioperations stored in @ssrc
 *
 *
 *
 */
void
seahorse_server_source_take_operation (SeahorseServerSource *ssrc, SeahorseOperation *op)
{
    g_return_if_fail (SEAHORSE_IS_SERVER_SOURCE (ssrc));
    g_return_if_fail (SEAHORSE_IS_OPERATION (op));
    
    seahorse_multi_operation_take (ssrc->priv->mop, op);
}

/* --------------------------------------------------------------------------
 * METHODS
 */

/**
* src: #SeahorseSource
*
* This function is a stub
*
* Returns NULL
**/
static SeahorseOperation*
seahorse_server_source_load (SeahorseSource *src)
{
    SeahorseServerSource *ssrc;
    
    g_assert (SEAHORSE_IS_SOURCE (src));
    ssrc = SEAHORSE_SERVER_SOURCE (src);

    /* We should never be called directly */
    return NULL;
}

/**
* uri: the uri to parse
* scheme: the scheme ("http") of this uri
* host: the host part of the uri
*
*
* Code adapted from GnuPG (file g10/keyserver.c)
*
* Returns FALSE if the separation failed
**/
static gboolean 
parse_keyserver_uri (char *uri, const char **scheme, const char **host)
{
    int assume_ldap = 0;

    g_assert (uri != NULL);
    g_assert (scheme != NULL && host != NULL);

    *scheme = NULL;
    *host = NULL;

    /* Get the scheme */

    *scheme = strsep(&uri, ":");
    if (uri == NULL) {
        /* Assume LDAP if there is no scheme */
        assume_ldap = 1;
        uri = (char*)*scheme;
        *scheme = "ldap";
    }

    if (assume_ldap || (uri[0] == '/' && uri[1] == '/')) {
        /* Two slashes means network path. */

        /* Skip over the "//", if any */
        if (!assume_ldap)
            uri += 2;

        /* Get the host */
        *host = strsep (&uri, "/");
        if (*host[0] == '\0')
            return FALSE;
    }

    if (*scheme[0] == '\0')
        return FALSE;

    return TRUE;
}

/**
 * seahorse_server_source_new:
 * @server: The server uri to create an object for
 *
 * Creates a #SeahorseServerSource object out of @server. Depending
 * on the defines at compilation time other sources are supported
 * (ldap, hkp)
 *
 * Returns: A new SeahorseServerSource or NULL
 */
SeahorseServerSource* 
seahorse_server_source_new (const gchar *server)
{
    SeahorseServerSource *ssrc = NULL;
    const gchar *scheme;
    const gchar *host;
    gchar *uri, *t;
    
    g_return_val_if_fail (server && server[0], NULL);
    
    uri = g_strdup (server);
        
    if (!parse_keyserver_uri (uri, &scheme, &host)) {
        g_warning ("invalid uri passed: %s", server);

        
    } else {
        
#ifdef WITH_LDAP       
        /* LDAP Uris */ 
        if (g_ascii_strcasecmp (scheme, "ldap") == 0) 
            ssrc = SEAHORSE_SERVER_SOURCE (seahorse_ldap_source_new (server, host));
        else
#endif /* WITH_LDAP */
        
#ifdef WITH_HKP
        /* HKP Uris */
        if (g_ascii_strcasecmp (scheme, "hkp") == 0) {
            
            ssrc = SEAHORSE_SERVER_SOURCE (seahorse_hkp_source_new (server, host));

        /* HTTP Uris */
        } else if (g_ascii_strcasecmp (scheme, "http") == 0 ||
                   g_ascii_strcasecmp (scheme, "https") == 0) {

            /* If already have a port */
            if (strchr (host, ':')) 
	            ssrc = SEAHORSE_SERVER_SOURCE (seahorse_hkp_source_new (server, host));

            /* No port make sure to use defaults */
            else {
                t = g_strdup_printf ("%s:%d", host, 
                                     (g_ascii_strcasecmp (scheme, "http") == 0) ? 80 : 443);
                ssrc = SEAHORSE_SERVER_SOURCE (seahorse_hkp_source_new (server, t));
                g_free (t);
            }

        } else
#endif /* WITH_HKP */
        
            g_warning ("unsupported key server uri scheme: %s", scheme);
    }
    
    g_free (uri);
    return ssrc;
}
