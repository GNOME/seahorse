
#include <seahorse-servers.h>
#include <seahorse-gconf.h>


#define SEAHORSE_SERVERS_TYPE_SERVER_INFO (seahorse_servers_server_info_get_type ())
#define SEAHORSE_SERVERS_SERVER_INFO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_SERVERS_TYPE_SERVER_INFO, SeahorseServersServerInfo))
#define SEAHORSE_SERVERS_SERVER_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_SERVERS_TYPE_SERVER_INFO, SeahorseServersServerInfoClass))
#define SEAHORSE_SERVERS_IS_SERVER_INFO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_SERVERS_TYPE_SERVER_INFO))
#define SEAHORSE_SERVERS_IS_SERVER_INFO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_SERVERS_TYPE_SERVER_INFO))
#define SEAHORSE_SERVERS_SERVER_INFO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_SERVERS_TYPE_SERVER_INFO, SeahorseServersServerInfoClass))

typedef struct _SeahorseServersServerInfo SeahorseServersServerInfo;
typedef struct _SeahorseServersServerInfoClass SeahorseServersServerInfoClass;
typedef struct _SeahorseServersServerInfoPrivate SeahorseServersServerInfoPrivate;

struct _SeahorseServersServerInfo {
	GTypeInstance parent_instance;
	volatile int ref_count;
	SeahorseServersServerInfoPrivate * priv;
	char* type;
	char* description;
	SeahorseValidUriFunc validator;
};

struct _SeahorseServersServerInfoClass {
	GTypeClass parent_class;
	void (*finalize) (SeahorseServersServerInfo *self);
};



enum  {
	SEAHORSE_SERVERS_DUMMY_PROPERTY
};
static GHashTable* seahorse_servers_types;
static void _g_slist_free_g_free (GSList* self);
enum  {
	SEAHORSE_SERVERS_SERVER_INFO_DUMMY_PROPERTY
};
static SeahorseServersServerInfo* seahorse_servers_server_info_new (void);
static gpointer seahorse_servers_server_info_parent_class = NULL;
static GType seahorse_servers_server_info_get_type (void);
static gpointer seahorse_servers_server_info_ref (gpointer instance);
static void seahorse_servers_server_info_unref (gpointer instance);
static gpointer seahorse_servers_parent_class = NULL;
static void seahorse_servers_dispose (GObject * obj);
static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func);



static void _g_slist_free_g_free (GSList* self) {
	g_slist_foreach (self, ((GFunc) (g_free)), NULL);
	g_slist_free (self);
}


GSList* seahorse_servers_get_types (void) {
	GSList* results;
	results = NULL;
	{
		GList* type_collection;
		GList* type_it;
		type_collection = g_hash_table_get_keys (seahorse_servers_types);
		for (type_it = type_collection; type_it != NULL; type_it = type_it->next) {
			const char* type;
			type = ((const char*) (type_it->data));
			{
				const char* _tmp0;
				_tmp0 = NULL;
				results = g_slist_append (results, (_tmp0 = type, (_tmp0 == NULL ? NULL : g_strdup (_tmp0))));
			}
		}
		(type_collection == NULL ? NULL : (type_collection = (g_list_free (type_collection), NULL)));
	}
	return results;
}


char* seahorse_servers_get_description (const char* type) {
	SeahorseServersServerInfo* _tmp0;
	SeahorseServersServerInfo* server;
	const char* _tmp2;
	char* _tmp3;
	g_return_val_if_fail (type != NULL, NULL);
	_tmp0 = NULL;
	server = (_tmp0 = ((SeahorseServersServerInfo*) (g_hash_table_lookup (seahorse_servers_types, type))), (_tmp0 == NULL ? NULL : seahorse_servers_server_info_ref (_tmp0)));
	if (server == NULL) {
		char* _tmp1;
		_tmp1 = NULL;
		return (_tmp1 = NULL, (server == NULL ? NULL : (server = (seahorse_servers_server_info_unref (server), NULL))), _tmp1);
	}
	_tmp2 = NULL;
	_tmp3 = NULL;
	return (_tmp3 = (_tmp2 = server->description, (_tmp2 == NULL ? NULL : g_strdup (_tmp2))), (server == NULL ? NULL : (server = (seahorse_servers_server_info_unref (server), NULL))), _tmp3);
}


void seahorse_servers_register_type (const char* type, const char* description, SeahorseValidUriFunc validate) {
	SeahorseServers* dummy;
	SeahorseServersServerInfo* info;
	char* _tmp1;
	const char* _tmp0;
	char* _tmp3;
	const char* _tmp2;
	SeahorseServersServerInfo* _tmp5;
	const char* _tmp4;
	g_return_if_fail (type != NULL);
	g_return_if_fail (description != NULL);
	/* Work around for: http://bugzilla.gnome.org/show_bug.cgi?id=543190 */
	dummy = seahorse_servers_new ();
	info = seahorse_servers_server_info_new ();
	_tmp1 = NULL;
	_tmp0 = NULL;
	info->type = (_tmp1 = (_tmp0 = type, (_tmp0 == NULL ? NULL : g_strdup (_tmp0))), (info->type = (g_free (info->type), NULL)), _tmp1);
	_tmp3 = NULL;
	_tmp2 = NULL;
	info->description = (_tmp3 = (_tmp2 = description, (_tmp2 == NULL ? NULL : g_strdup (_tmp2))), (info->description = (g_free (info->description), NULL)), _tmp3);
	info->validator = validate;
	_tmp5 = NULL;
	_tmp4 = NULL;
	g_hash_table_replace (seahorse_servers_types, (_tmp4 = type, (_tmp4 == NULL ? NULL : g_strdup (_tmp4))), (_tmp5 = info, (_tmp5 == NULL ? NULL : seahorse_servers_server_info_ref (_tmp5))));
	(dummy == NULL ? NULL : (dummy = (g_object_unref (dummy), NULL)));
	(info == NULL ? NULL : (info = (seahorse_servers_server_info_unref (info), NULL)));
}


GSList* seahorse_servers_get_uris (void) {
	GSList* servers;
	GSList* results;
	GSList* _tmp3;
	servers = seahorse_gconf_get_string_list (KEYSERVER_KEY);
	results = NULL;
	/* The values are 'uri name', remove the name part */
	{
		GSList* value_collection;
		GSList* value_it;
		value_collection = servers;
		for (value_it = value_collection; value_it != NULL; value_it = value_it->next) {
			const char* _tmp2;
			char* value;
			_tmp2 = NULL;
			value = (_tmp2 = ((char*) (value_it->data)), (_tmp2 == NULL ? NULL : g_strdup (_tmp2)));
			{
				char** _tmp0;
				gint parts_length1;
				char** parts;
				const char* _tmp1;
				_tmp0 = NULL;
				parts = (_tmp0 = g_strsplit (value, " ", 2), parts_length1 = -1, _tmp0);
				_tmp1 = NULL;
				results = g_slist_append (results, (_tmp1 = parts[0], (_tmp1 == NULL ? NULL : g_strdup (_tmp1))));
				value = (g_free (value), NULL);
				parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL);
			}
		}
	}
	_tmp3 = NULL;
	return (_tmp3 = results, (servers == NULL ? NULL : (servers = (_g_slist_free_g_free (servers), NULL))), _tmp3);
}


GSList* seahorse_servers_get_names (void) {
	GSList* servers;
	GSList* results;
	GSList* _tmp4;
	servers = seahorse_gconf_get_string_list (KEYSERVER_KEY);
	results = NULL;
	/* The values are 'uri name', remove the uri part */
	{
		GSList* value_collection;
		GSList* value_it;
		value_collection = servers;
		for (value_it = value_collection; value_it != NULL; value_it = value_it->next) {
			const char* _tmp3;
			char* value;
			_tmp3 = NULL;
			value = (_tmp3 = ((char*) (value_it->data)), (_tmp3 == NULL ? NULL : g_strdup (_tmp3)));
			{
				char** _tmp0;
				gint parts_length1;
				char** parts;
				_tmp0 = NULL;
				parts = (_tmp0 = g_strsplit (value, " ", 2), parts_length1 = -1, _tmp0);
				if (parts_length1 == 2 && g_utf8_strlen (parts[1], -1) > 0) {
					const char* _tmp1;
					_tmp1 = NULL;
					results = g_slist_append (results, (_tmp1 = parts[1], (_tmp1 == NULL ? NULL : g_strdup (_tmp1))));
				} else {
					const char* _tmp2;
					_tmp2 = NULL;
					results = g_slist_append (results, (_tmp2 = parts[0], (_tmp2 == NULL ? NULL : g_strdup (_tmp2))));
				}
				value = (g_free (value), NULL);
				parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL);
			}
		}
	}
	_tmp4 = NULL;
	return (_tmp4 = results, (servers == NULL ? NULL : (servers = (_g_slist_free_g_free (servers), NULL))), _tmp4);
}


/* Check to see if the passed uri is valid against registered validators */
gboolean seahorse_servers_is_valid_uri (const char* uri) {
	char** _tmp0;
	gint parts_length1;
	char** parts;
	SeahorseServersServerInfo* _tmp2;
	SeahorseServersServerInfo* info;
	gboolean _tmp4;
	g_return_val_if_fail (uri != NULL, FALSE);
	_tmp0 = NULL;
	parts = (_tmp0 = g_strsplit (uri, ":", 2), parts_length1 = -1, _tmp0);
	if (parts_length1 == 0) {
		gboolean _tmp1;
		return (_tmp1 = FALSE, (parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL)), _tmp1);
	}
	_tmp2 = NULL;
	info = (_tmp2 = ((SeahorseServersServerInfo*) (g_hash_table_lookup (seahorse_servers_types, parts[0]))), (_tmp2 == NULL ? NULL : seahorse_servers_server_info_ref (_tmp2)));
	if (info == NULL) {
		gboolean _tmp3;
		return (_tmp3 = FALSE, (parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL)), (info == NULL ? NULL : (info = (seahorse_servers_server_info_unref (info), NULL))), _tmp3);
	}
	return (_tmp4 = info->validator (uri), (parts = (_vala_array_free (parts, parts_length1, ((GDestroyNotify) (g_free))), NULL)), (info == NULL ? NULL : (info = (seahorse_servers_server_info_unref (info), NULL))), _tmp4);
}


SeahorseServers* seahorse_servers_new (void) {
	SeahorseServers * self;
	self = g_object_newv (SEAHORSE_TYPE_SERVERS, 0, NULL);
	return self;
}


static SeahorseServersServerInfo* seahorse_servers_server_info_new (void) {
	SeahorseServersServerInfo* self;
	self = ((SeahorseServersServerInfo*) (g_type_create_instance (SEAHORSE_SERVERS_TYPE_SERVER_INFO)));
	return self;
}


static void seahorse_servers_server_info_class_init (SeahorseServersServerInfoClass * klass) {
	seahorse_servers_server_info_parent_class = g_type_class_peek_parent (klass);
}


static void seahorse_servers_server_info_instance_init (SeahorseServersServerInfo * self) {
	self->ref_count = 1;
}


static GType seahorse_servers_server_info_get_type (void) {
	static GType seahorse_servers_server_info_type_id = 0;
	if (G_UNLIKELY (seahorse_servers_server_info_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseServersServerInfoClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_servers_server_info_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseServersServerInfo), 0, (GInstanceInitFunc) seahorse_servers_server_info_instance_init };
		static const GTypeFundamentalInfo g_define_type_fundamental_info = { (G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_INSTANTIATABLE | G_TYPE_FLAG_DERIVABLE | G_TYPE_FLAG_DEEP_DERIVABLE) };
		seahorse_servers_server_info_type_id = g_type_register_fundamental (g_type_fundamental_next (), "SeahorseServersServerInfo", &g_define_type_info, &g_define_type_fundamental_info, 0);
	}
	return seahorse_servers_server_info_type_id;
}


static gpointer seahorse_servers_server_info_ref (gpointer instance) {
	SeahorseServersServerInfo* self;
	self = instance;
	g_atomic_int_inc (&self->ref_count);
	return instance;
}


static void seahorse_servers_server_info_unref (gpointer instance) {
	SeahorseServersServerInfo* self;
	self = instance;
	if (g_atomic_int_dec_and_test (&self->ref_count)) {
		g_type_free_instance (((GTypeInstance *) (self)));
	}
}


static void seahorse_servers_class_init (SeahorseServersClass * klass) {
	seahorse_servers_parent_class = g_type_class_peek_parent (klass);
	G_OBJECT_CLASS (klass)->dispose = seahorse_servers_dispose;
	{
		GHashTable* _tmp0;
		/* TODO: What do we specify to free ServerInfo? */
		_tmp0 = NULL;
		seahorse_servers_types = (_tmp0 = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL), (seahorse_servers_types == NULL ? NULL : (seahorse_servers_types = (g_hash_table_unref (seahorse_servers_types), NULL))), _tmp0);
	}
}


static void seahorse_servers_instance_init (SeahorseServers * self) {
}


static void seahorse_servers_dispose (GObject * obj) {
	SeahorseServers * self;
	self = SEAHORSE_SERVERS (obj);
	G_OBJECT_CLASS (seahorse_servers_parent_class)->dispose (obj);
}


GType seahorse_servers_get_type (void) {
	static GType seahorse_servers_type_id = 0;
	if (G_UNLIKELY (seahorse_servers_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorseServersClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_servers_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorseServers), 0, (GInstanceInitFunc) seahorse_servers_instance_init };
		seahorse_servers_type_id = g_type_register_static (G_TYPE_OBJECT, "SeahorseServers", &g_define_type_info, 0);
	}
	return seahorse_servers_type_id;
}


static void _vala_array_free (gpointer array, gint array_length, GDestroyNotify destroy_func) {
	if (array != NULL && destroy_func != NULL) {
		int i;
		if (array_length >= 0)
		for (i = 0; i < array_length; i = i + 1) {
			if (((gpointer*) (array))[i] != NULL)
			destroy_func (((gpointer*) (array))[i]);
		}
		else
		for (i = 0; ((gpointer*) (array))[i] != NULL; i = i + 1) {
			destroy_func (((gpointer*) (array))[i]);
		}
	}
	g_free (array);
}




