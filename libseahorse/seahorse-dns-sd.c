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

#include <gnome.h>

#ifdef WITH_SHARING 
/* Workaround broken howl installing config.h */
#undef PACKAGE
#undef VERSION
#undef PACKAGE_STRING
#undef PACKAGE_NAME
#undef PACKAGE_VERSION
#undef PACKAGE_TARNAME
#include <howl.h>
#endif /* WITH_SHARING */

#include "seahorse-dns-sd.h"
#include "seahorse-context.h"
#include "seahorse-server-source.h"

/* Override the DEBUG_DNSSD_ENABLE switch here */
/* #define DEBUG_DNSSD_ENABLE 1 */

#ifndef DEBUG_DNSSD_ENABLE
#if _DEBUG
#define DEBUG_DNSSD_ENABLE 1
#else
#define DEBUG_DNSSD_ENABLE 0
#endif
#endif

#if DEBUG_DNSSD_ENABLE
#define DEBUG_DNSSD(x) g_printerr x
#else
#define DEBUG_DNSSD(x) 
#endif

#define HKP_SERVICE_TYPE "_hkp._tcp."

/* 
 * DEBUG: Define if we want to show discoveries from this
 * host. Normally these are suppressed 
 */
/* #define DISCOVER_THIS_HOST 1 */

struct _SeahorseServiceDiscoveryPriv {
#ifdef WITH_SHARING
    sw_discovery session;
    sw_discovery_oid  oid;
#endif
    guint timer;
};

enum {
    ADDED,
    REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

/* GObject handlers */
static void class_init        (SeahorseServiceDiscoveryClass *ssd);
static void object_init       (SeahorseServiceDiscovery *ssd);
static void object_dispose    (GObject *gobject);
static void object_finalize   (GObject *gobject);

GType
seahorse_service_discovery_get_type (void) {
    static GType gtype = 0;
 
    if (!gtype) {
        
        static const GTypeInfo gtinfo = {
            sizeof (SeahorseServiceDiscoveryClass), NULL, NULL,
            (GClassInitFunc) class_init, NULL, NULL,
            sizeof (SeahorseServiceDiscovery), 0, (GInstanceInitFunc) object_init
        };
        
        gtype = g_type_register_static (G_TYPE_OBJECT, "SeahorseServiceDiscovery", 
                                        &gtinfo, 0);
    }
  
    return gtype;
}

static void
class_init (SeahorseServiceDiscoveryClass *klass)
{
    GObjectClass *gobject_class;
   
    parent_class = g_type_class_peek_parent (klass);
    gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->dispose = object_dispose;
    gobject_class->finalize = object_finalize;

    signals[ADDED] = g_signal_new ("added", SEAHORSE_TYPE_SERVICE_DISCOVERY, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseServiceDiscoveryClass, added),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
    signals[REMOVED] = g_signal_new ("removed", SEAHORSE_TYPE_SERVICE_DISCOVERY, 
                G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (SeahorseServiceDiscoveryClass, removed),
                NULL, NULL, g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

#ifdef WITH_SHARING

static gboolean     
salt_timer (SeahorseServiceDiscovery *ssd)
{
    sw_salt salt;
    sw_ulong timeout = 0;
    
    g_return_val_if_fail (SEAHORSE_IS_SERVICE_DISCOVERY (ssd), FALSE);
    g_return_val_if_fail (ssd->priv->session != NULL, FALSE);
    sw_discovery_salt (ssd->priv->session, &salt);
    sw_salt_step (salt, &timeout);
    
    return TRUE;
}

static sw_result
resolve_callback (sw_discovery discovery, sw_discovery_oid oid, sw_uint32 interface_index,
                  sw_const_string name, sw_const_string type, sw_const_string domain,
                  sw_ipv4_address address, sw_port port, sw_octets text_record, 
                  sw_ulong text_record_len, SeahorseServiceDiscovery *ssd)
{
    sw_ipv4_address local;
    gchar *service_name;
    gchar *service_uri;
    gchar *ipname;
    
    /* One address is enough */
    sw_discovery_cancel (discovery, oid);   
    
    g_return_val_if_fail (SEAHORSE_IS_SERVICE_DISCOVERY (ssd), SW_DISCOVERY_E_BAD_PARAM);

    if (g_strcasecmp (HKP_SERVICE_TYPE, type) != 0)
        return SW_OKAY;
    
    /* To prevent warnings */
    memset (&local, 0, sizeof (local));

#ifndef DISCOVER_THIS_HOST

    /* 
     * TODO: In the future there needs to be a better way of determining 
     * which services we see that are from the local host. This can fail
     * on multihomed hosts.
     */
    sw_ipv4_address_init_from_this_host (&local);
    if (sw_ipv4_address_equals (local, address))
        return SW_OKAY;
    
#endif /* DISCOVER_THIS_HOST */
    
    ipname = g_new0(gchar, 16);
    sw_ipv4_address_name (address, ipname, 16);
    service_uri = g_strdup_printf ("hkp://%s:%d", ipname, port);
    service_name = g_strdup (name);
    
    g_hash_table_replace (ssd->services, service_name, service_uri);
    g_signal_emit (ssd, signals[ADDED], 0, service_name);
    
    /* Add it to the context */
    if (!seahorse_context_remote_key_source (SCTX_APP (), service_uri)) {
        SeahorseServerSource *ssrc = seahorse_server_source_new (service_uri);
        g_return_val_if_fail (ssrc != NULL, SW_OKAY);
        seahorse_context_add_key_source (SCTX_APP (), SEAHORSE_KEY_SOURCE (ssrc));
    }
    
    DEBUG_DNSSD (("DNS-SD added: %s %s\n", service_name, service_uri));
    
    return SW_OKAY;    
}

static sw_result
browse_callback (sw_discovery discovery, sw_discovery_oid id,
                 sw_discovery_browse_status status, sw_uint32 interface_index,
                 sw_const_string name, sw_const_string type, sw_const_string domain,
                 SeahorseServiceDiscovery *ssd)
{
    sw_discovery_oid oid;
    const gchar *uri;

    g_return_val_if_fail (SEAHORSE_IS_SERVICE_DISCOVERY (ssd), SW_DISCOVERY_E_BAD_PARAM);
    
    if (g_strcasecmp (HKP_SERVICE_TYPE, type) != 0)
        return SW_OKAY;

    if (status == SW_DISCOVERY_BROWSE_ADD_SERVICE) {
        
        if (sw_discovery_resolve (ssd->priv->session, 0, name, type, domain, 
                                  (sw_discovery_resolve_reply)resolve_callback, 
                                   ssd, &oid) != SW_OKAY) 
            g_warning ("error resolving DNS-SD name: %s", name);
        
    } else if (status == SW_DISCOVERY_BROWSE_REMOVE_SERVICE) {
        
        /* Remove it from the context */
        uri = g_hash_table_lookup (ssd->services, name);
        if (uri != NULL) {
            SeahorseKeySource *sksrc = seahorse_context_remote_key_source (SCTX_APP(), uri);
            seahorse_context_remove_key_source (SCTX_APP(), sksrc);
        }

        g_hash_table_remove (ssd->services, name);
        g_signal_emit (ssd, signals[REMOVED], 0, name);
        DEBUG_DNSSD (("DNS-SD removed: %s\n", name));
        
    }
    
    return SW_OKAY;
}

#endif /* WITH_SHARING */

static void
object_init (SeahorseServiceDiscovery *ssd)
{
	ssd->priv = g_new0 (SeahorseServiceDiscoveryPriv, 1);
    ssd->services = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    
#ifdef WITH_SHARING     
    if (sw_discovery_init (&(ssd->priv->session)) != SW_OKAY) {
        g_warning ("DNS-SD initialization failed");
        ssd->priv->session = NULL;
        return;
    }
        
    if (sw_discovery_browse (ssd->priv->session, 0, HKP_SERVICE_TYPE, NULL,
					         (sw_discovery_browse_reply)browse_callback, ssd, 
                             &(ssd->priv->oid)) != SW_OKAY) {
        g_warning ("Browsing for DNS-SD services failed");
        ssd->priv->oid = 0;
    }
    
    ssd->priv->timer = g_timeout_add (100, (GSourceFunc)salt_timer, ssd);
#endif /* WITH_SHARING */
}

static void
object_dispose (GObject *gobject)
{
    SeahorseServiceDiscovery *ssd;
    ssd = SEAHORSE_SERVICE_DISCOVERY (gobject);
    
#ifdef WITH_SHARING     
    if (ssd->priv->oid != 0 && ssd->priv->session != NULL)
        sw_discovery_cancel (ssd->priv->session, ssd->priv->oid);
    ssd->priv->oid = 0;
    
    if (ssd->priv->session != NULL)
        sw_discovery_fina (ssd->priv->session);
    ssd->priv->session = NULL;
    
    if (ssd->priv->timer != 0)
        g_source_remove (ssd->priv->timer);
    ssd->priv->timer = 0;
#endif /* WITH_SHARING */
    
    G_OBJECT_CLASS (parent_class)->dispose (gobject);
}

/* free private vars */
static void
object_finalize (GObject *gobject)
{
    SeahorseServiceDiscovery *ssd = SEAHORSE_SERVICE_DISCOVERY (gobject);
  
#ifdef WITH_SHARING    
    g_assert (ssd->priv->session == NULL);
    g_assert (ssd->priv->oid == 0);
    g_assert (ssd->priv->timer == 0);
#endif /* WITH_SHARING */    
    
    g_hash_table_destroy (ssd->services);
    ssd->services = NULL;
    
    g_free (ssd->priv);
    ssd->priv = NULL;
    
    G_OBJECT_CLASS (parent_class)->finalize (gobject);
}


SeahorseServiceDiscovery*   
seahorse_service_discovery_new ()
{
    return g_object_new (SEAHORSE_TYPE_SERVICE_DISCOVERY, NULL);
}

static void
service_key_list (const gchar* key, const gchar* value, GSList **arg)
{
    *arg = g_slist_prepend (*arg, g_strdup (key));
}

GSList*                     
seahorse_service_discovery_list (SeahorseServiceDiscovery *ssd)
{
    GSList *list = NULL;
    GSList **arg = &list;
    
    g_return_val_if_fail (SEAHORSE_IS_SERVICE_DISCOVERY (ssd), NULL);
    g_hash_table_foreach (ssd->services, (GHFunc)service_key_list, arg);
    return *arg;
}

const gchar*                
seahorse_service_discovery_get_uri (SeahorseServiceDiscovery *ssd, const gchar *service)
{
    g_return_val_if_fail (SEAHORSE_IS_SERVICE_DISCOVERY (ssd), NULL);
    return (const gchar*)g_hash_table_lookup (ssd->services, service);
}

GSList*
seahorse_service_discovery_get_uris (SeahorseServiceDiscovery *ssd, GSList *services)
{
    GSList *uris = NULL;
    const gchar *uri;
    
    while (services) {
        uri = (const gchar*)g_hash_table_lookup (ssd->services, services->data);    
        uris = g_slist_append (uris, uri ? g_strdup (uri) : NULL);
        services = g_slist_next (services);
    }

    return uris;
}
