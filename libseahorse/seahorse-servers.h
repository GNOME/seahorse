
#ifndef __SEAHORSE_SERVERS_H__
#define __SEAHORSE_SERVERS_H__

#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>

G_BEGIN_DECLS

typedef gboolean (*SeahorseValidUriFunc) (const char* uri);

#define SEAHORSE_TYPE_SERVERS (seahorse_servers_get_type ())
#define SEAHORSE_SERVERS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_TYPE_SERVERS, SeahorseServers))
#define SEAHORSE_SERVERS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_SERVERS, SeahorseServersClass))
#define SEAHORSE_IS_SERVERS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_TYPE_SERVERS))
#define SEAHORSE_IS_SERVERS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_SERVERS))
#define SEAHORSE_SERVERS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_TYPE_SERVERS, SeahorseServersClass))

typedef struct _SeahorseServers SeahorseServers;
typedef struct _SeahorseServersClass SeahorseServersClass;
typedef struct _SeahorseServersPrivate SeahorseServersPrivate;

struct _SeahorseServers {
	GObject parent_instance;
	SeahorseServersPrivate * priv;
};

struct _SeahorseServersClass {
	GObjectClass parent_class;
};


GSList* seahorse_servers_get_types (void);
char* seahorse_servers_get_description (const char* type);
void seahorse_servers_register_type (const char* type, const char* description, SeahorseValidUriFunc validate);
GSList* seahorse_servers_get_uris (void);
GSList* seahorse_servers_get_names (void);
gboolean seahorse_servers_is_valid_uri (const char* uri);
SeahorseServers* seahorse_servers_new (void);
GType seahorse_servers_get_type (void);


G_END_DECLS

#endif
