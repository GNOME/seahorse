
#include "seahorse-pkcs11-source.h"
#include <pkcs11.h>
#include <pkcs11g.h>
#include <seahorse-util.h>
#include <glib/gi18n-lib.h>
#include <seahorse-context.h>
#include "seahorse-pkcs11.h"
#include "seahorse-pkcs11-certificate.h"


#define SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER (seahorse_pkcs11_source_updater_get_type ())
#define SEAHORSE_PKCS11_SOURCE_UPDATER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER, SeahorsePkcs11SourceUpdater))
#define SEAHORSE_PKCS11_SOURCE_UPDATER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER, SeahorsePkcs11SourceUpdaterClass))
#define SEAHORSE_PKCS11_SOURCE_IS_UPDATER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER))
#define SEAHORSE_PKCS11_SOURCE_IS_UPDATER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER))
#define SEAHORSE_PKCS11_SOURCE_UPDATER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER, SeahorsePkcs11SourceUpdaterClass))

typedef struct _SeahorsePkcs11SourceUpdater SeahorsePkcs11SourceUpdater;
typedef struct _SeahorsePkcs11SourceUpdaterClass SeahorsePkcs11SourceUpdaterClass;
typedef struct _SeahorsePkcs11SourceUpdaterPrivate SeahorsePkcs11SourceUpdaterPrivate;

#define SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER (seahorse_pkcs11_source_refresher_get_type ())
#define SEAHORSE_PKCS11_SOURCE_REFRESHER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER, SeahorsePkcs11SourceRefresher))
#define SEAHORSE_PKCS11_SOURCE_REFRESHER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER, SeahorsePkcs11SourceRefresherClass))
#define SEAHORSE_PKCS11_SOURCE_IS_REFRESHER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER))
#define SEAHORSE_PKCS11_SOURCE_IS_REFRESHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER))
#define SEAHORSE_PKCS11_SOURCE_REFRESHER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER, SeahorsePkcs11SourceRefresherClass))

typedef struct _SeahorsePkcs11SourceRefresher SeahorsePkcs11SourceRefresher;
typedef struct _SeahorsePkcs11SourceRefresherClass SeahorsePkcs11SourceRefresherClass;
typedef struct _SeahorsePkcs11SourceRefresherPrivate SeahorsePkcs11SourceRefresherPrivate;

#define SEAHORSE_PKCS11_SOURCE_TYPE_LOADER (seahorse_pkcs11_source_loader_get_type ())
#define SEAHORSE_PKCS11_SOURCE_LOADER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_LOADER, SeahorsePkcs11SourceLoader))
#define SEAHORSE_PKCS11_SOURCE_LOADER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_LOADER, SeahorsePkcs11SourceLoaderClass))
#define SEAHORSE_PKCS11_SOURCE_IS_LOADER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_LOADER))
#define SEAHORSE_PKCS11_SOURCE_IS_LOADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_LOADER))
#define SEAHORSE_PKCS11_SOURCE_LOADER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_LOADER, SeahorsePkcs11SourceLoaderClass))

typedef struct _SeahorsePkcs11SourceLoader SeahorsePkcs11SourceLoader;
typedef struct _SeahorsePkcs11SourceLoaderClass SeahorsePkcs11SourceLoaderClass;
typedef struct _SeahorsePkcs11SourceLoaderPrivate SeahorsePkcs11SourceLoaderPrivate;

#define SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER (seahorse_pkcs11_source_importer_get_type ())
#define SEAHORSE_PKCS11_SOURCE_IMPORTER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER, SeahorsePkcs11SourceImporter))
#define SEAHORSE_PKCS11_SOURCE_IMPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER, SeahorsePkcs11SourceImporterClass))
#define SEAHORSE_PKCS11_SOURCE_IS_IMPORTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER))
#define SEAHORSE_PKCS11_SOURCE_IS_IMPORTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER))
#define SEAHORSE_PKCS11_SOURCE_IMPORTER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER, SeahorsePkcs11SourceImporterClass))

typedef struct _SeahorsePkcs11SourceImporter SeahorsePkcs11SourceImporter;
typedef struct _SeahorsePkcs11SourceImporterClass SeahorsePkcs11SourceImporterClass;
typedef struct _SeahorsePkcs11SourceImporterPrivate SeahorsePkcs11SourceImporterPrivate;

#define SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER (seahorse_pkcs11_source_remover_get_type ())
#define SEAHORSE_PKCS11_SOURCE_REMOVER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER, SeahorsePkcs11SourceRemover))
#define SEAHORSE_PKCS11_SOURCE_REMOVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER, SeahorsePkcs11SourceRemoverClass))
#define SEAHORSE_PKCS11_SOURCE_IS_REMOVER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER))
#define SEAHORSE_PKCS11_SOURCE_IS_REMOVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER))
#define SEAHORSE_PKCS11_SOURCE_REMOVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER, SeahorsePkcs11SourceRemoverClass))

typedef struct _SeahorsePkcs11SourceRemover SeahorsePkcs11SourceRemover;
typedef struct _SeahorsePkcs11SourceRemoverClass SeahorsePkcs11SourceRemoverClass;
typedef struct _SeahorsePkcs11SourceRemoverPrivate SeahorsePkcs11SourceRemoverPrivate;

/* ------------------------------------------------------------------------------
 * Base class for several operations that all load objects (refresh, load, import)
 */
struct _SeahorsePkcs11SourceUpdater {
	SeahorseOperation parent_instance;
	SeahorsePkcs11SourceUpdaterPrivate * priv;
	SeahorsePkcs11Source* _source;
	GP11Session* _session;
};

struct _SeahorsePkcs11SourceUpdaterClass {
	SeahorseOperationClass parent_class;
	void (*load_objects) (SeahorsePkcs11SourceUpdater* self);
};

/* ------------------------------------------------------------------------------- 
 * REFRESH OPREATION
 */
struct _SeahorsePkcs11SourceRefresher {
	SeahorsePkcs11SourceUpdater parent_instance;
	SeahorsePkcs11SourceRefresherPrivate * priv;
};

struct _SeahorsePkcs11SourceRefresherClass {
	SeahorsePkcs11SourceUpdaterClass parent_class;
};

/* ------------------------------------------------------------------------------- 
 * LOAD OPREATION
 */
struct _SeahorsePkcs11SourceLoader {
	SeahorsePkcs11SourceUpdater parent_instance;
	SeahorsePkcs11SourceLoaderPrivate * priv;
};

struct _SeahorsePkcs11SourceLoaderClass {
	SeahorsePkcs11SourceUpdaterClass parent_class;
};

/* ------------------------------------------------------------------------------- 
 * IMPORT OPREATION
 */
struct _SeahorsePkcs11SourceImporter {
	SeahorsePkcs11SourceUpdater parent_instance;
	SeahorsePkcs11SourceImporterPrivate * priv;
};

struct _SeahorsePkcs11SourceImporterClass {
	SeahorsePkcs11SourceUpdaterClass parent_class;
};

/* ------------------------------------------------------------------------------- 
 * REMOVE OPREATION
 */
struct _SeahorsePkcs11SourceRemover {
	SeahorseOperation parent_instance;
	SeahorsePkcs11SourceRemoverPrivate * priv;
};

struct _SeahorsePkcs11SourceRemoverClass {
	SeahorseOperationClass parent_class;
};



struct _SeahorsePkcs11SourcePrivate {
	GP11Slot* _slot;
};

#define SEAHORSE_PKCS11_SOURCE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_TYPE_SOURCE, SeahorsePkcs11SourcePrivate))
enum  {
	SEAHORSE_PKCS11_SOURCE_DUMMY_PROPERTY,
	SEAHORSE_PKCS11_SOURCE_SLOT,
	SEAHORSE_PKCS11_SOURCE_LOCATION,
	SEAHORSE_PKCS11_SOURCE_KEY_TYPE,
	SEAHORSE_PKCS11_SOURCE_KEY_DESC
};
static SeahorseOperation* seahorse_pkcs11_source_real_load (SeahorseSource* base, GQuark id);
static SeahorseOperation* seahorse_pkcs11_source_real_import (SeahorsePkcs11Source* self, GInputStream* input);
static SeahorseOperation* seahorse_pkcs11_source_real_export (SeahorsePkcs11Source* self, GList* objects, GOutputStream* output);
static SeahorseOperation* seahorse_pkcs11_source_real_remove (SeahorsePkcs11Source* self, SeahorseObject* object);
static void seahorse_pkcs11_source_receive_object (SeahorsePkcs11Source* self, GP11Object* object, GP11Attributes* attrs);
static void seahorse_pkcs11_source_set_slot (SeahorsePkcs11Source* self, GP11Slot* value);
struct _SeahorsePkcs11SourceUpdaterPrivate {
	GCancellable* _cancellable;
	GList* _objects;
	gint _total;
};

#define SEAHORSE_PKCS11_SOURCE_UPDATER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER, SeahorsePkcs11SourceUpdaterPrivate))
enum  {
	SEAHORSE_PKCS11_SOURCE_UPDATER_DUMMY_PROPERTY,
	SEAHORSE_PKCS11_SOURCE_UPDATER_CANCELLABLE,
	SEAHORSE_PKCS11_SOURCE_UPDATER_SOURCE
};
static void _g_list_free_g_object_unref (GList* self);
static void seahorse_pkcs11_source_updater_complete (SeahorsePkcs11SourceUpdater* self, GError* err);
static void seahorse_pkcs11_source_updater_on_open_session (SeahorsePkcs11SourceUpdater* self, GObject* obj, GAsyncResult* result);
static void seahorse_pkcs11_source_updater_real_load_objects (SeahorsePkcs11SourceUpdater* self);
static void seahorse_pkcs11_source_updater_load_objects (SeahorsePkcs11SourceUpdater* self);
static void seahorse_pkcs11_source_updater_loaded_objects (SeahorsePkcs11SourceUpdater* self, GList* objects);
static void _seahorse_pkcs11_source_updater_on_get_attributes_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self);
static void seahorse_pkcs11_source_updater_do_get_attributes (SeahorsePkcs11SourceUpdater* self);
static void seahorse_pkcs11_source_updater_on_get_attributes (SeahorsePkcs11SourceUpdater* self, GObject* obj, GAsyncResult* result);
static void seahorse_pkcs11_source_updater_real_cancel (SeahorseOperation* base);
static GCancellable* seahorse_pkcs11_source_updater_get_cancellable (SeahorsePkcs11SourceUpdater* self);
static SeahorsePkcs11Source* seahorse_pkcs11_source_updater_get_source (SeahorsePkcs11SourceUpdater* self);
static void seahorse_pkcs11_source_updater_set_source (SeahorsePkcs11SourceUpdater* self, SeahorsePkcs11Source* value);
static void _seahorse_pkcs11_source_updater_on_open_session_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self);
static GObject * seahorse_pkcs11_source_updater_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_pkcs11_source_updater_parent_class = NULL;
static void seahorse_pkcs11_source_updater_dispose (GObject * obj);
static GType seahorse_pkcs11_source_updater_get_type (void);
struct _SeahorsePkcs11SourceRefresherPrivate {
	GHashTable* _checks;
};

#define SEAHORSE_PKCS11_SOURCE_REFRESHER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER, SeahorsePkcs11SourceRefresherPrivate))
enum  {
	SEAHORSE_PKCS11_SOURCE_REFRESHER_DUMMY_PROPERTY
};
static SeahorsePkcs11SourceRefresher* seahorse_pkcs11_source_refresher_new (SeahorsePkcs11Source* source);
static void _seahorse_pkcs11_source_refresher_on_find_objects_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self);
static void seahorse_pkcs11_source_refresher_real_load_objects (SeahorsePkcs11SourceUpdater* base);
static void __lambda0 (void* k, void* v, SeahorsePkcs11SourceRefresher* self);
static void ___lambda0_gh_func (void* key, void* value, gpointer self);
static void seahorse_pkcs11_source_refresher_on_find_objects (SeahorsePkcs11SourceRefresher* self, GObject* obj, GAsyncResult* result);
static GObject * seahorse_pkcs11_source_refresher_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_pkcs11_source_refresher_parent_class = NULL;
static void seahorse_pkcs11_source_refresher_dispose (GObject * obj);
static GType seahorse_pkcs11_source_refresher_get_type (void);
struct _SeahorsePkcs11SourceLoaderPrivate {
	GP11Attributes* _unique_attrs;
};

#define SEAHORSE_PKCS11_SOURCE_LOADER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_SOURCE_TYPE_LOADER, SeahorsePkcs11SourceLoaderPrivate))
enum  {
	SEAHORSE_PKCS11_SOURCE_LOADER_DUMMY_PROPERTY,
	SEAHORSE_PKCS11_SOURCE_LOADER_UNIQUE_ATTRS
};
static SeahorsePkcs11SourceLoader* seahorse_pkcs11_source_loader_new (SeahorsePkcs11Source* source, GP11Attributes* unique_attrs);
static void _seahorse_pkcs11_source_loader_on_find_objects_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self);
static void seahorse_pkcs11_source_loader_real_load_objects (SeahorsePkcs11SourceUpdater* base);
static void seahorse_pkcs11_source_loader_on_find_objects (SeahorsePkcs11SourceLoader* self, GObject* obj, GAsyncResult* result);
static GP11Attributes* seahorse_pkcs11_source_loader_get_unique_attrs (SeahorsePkcs11SourceLoader* self);
static void seahorse_pkcs11_source_loader_set_unique_attrs (SeahorsePkcs11SourceLoader* self, GP11Attributes* value);
static gpointer seahorse_pkcs11_source_loader_parent_class = NULL;
static void seahorse_pkcs11_source_loader_dispose (GObject * obj);
static GType seahorse_pkcs11_source_loader_get_type (void);
struct _SeahorsePkcs11SourceImporterPrivate {
	GP11Attributes* _import_data;
};

#define SEAHORSE_PKCS11_SOURCE_IMPORTER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER, SeahorsePkcs11SourceImporterPrivate))
enum  {
	SEAHORSE_PKCS11_SOURCE_IMPORTER_DUMMY_PROPERTY,
	SEAHORSE_PKCS11_SOURCE_IMPORTER_IMPORT_DATA
};
static SeahorsePkcs11SourceImporter* seahorse_pkcs11_source_importer_new (SeahorsePkcs11Source* source, GP11Attributes* import);
static void _seahorse_pkcs11_source_importer_on_create_object_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self);
static void seahorse_pkcs11_source_importer_real_load_objects (SeahorsePkcs11SourceUpdater* base);
static void _seahorse_pkcs11_source_importer_on_get_attribute_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self);
static void seahorse_pkcs11_source_importer_on_create_object (SeahorsePkcs11SourceImporter* self, GObject* obj, GAsyncResult* result);
static void seahorse_pkcs11_source_importer_on_get_attribute (SeahorsePkcs11SourceImporter* self, GObject* obj, GAsyncResult* result);
static GP11Attributes* seahorse_pkcs11_source_importer_get_import_data (SeahorsePkcs11SourceImporter* self);
static gpointer seahorse_pkcs11_source_importer_parent_class = NULL;
static void seahorse_pkcs11_source_importer_dispose (GObject * obj);
static GType seahorse_pkcs11_source_importer_get_type (void);
struct _SeahorsePkcs11SourceRemoverPrivate {
	GCancellable* _cancellable;
	SeahorsePkcs11Source* _source;
	SeahorsePkcs11Certificate* _certificate;
};

#define SEAHORSE_PKCS11_SOURCE_REMOVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER, SeahorsePkcs11SourceRemoverPrivate))
enum  {
	SEAHORSE_PKCS11_SOURCE_REMOVER_DUMMY_PROPERTY,
	SEAHORSE_PKCS11_SOURCE_REMOVER_CANCELLABLE,
	SEAHORSE_PKCS11_SOURCE_REMOVER_SOURCE,
	SEAHORSE_PKCS11_SOURCE_REMOVER_CERTIFICATE
};
static SeahorsePkcs11SourceRemover* seahorse_pkcs11_source_remover_new (SeahorsePkcs11Certificate* certificate);
static void seahorse_pkcs11_source_remover_on_destroy_object (SeahorsePkcs11SourceRemover* self, GObject* obj, GAsyncResult* result);
static void seahorse_pkcs11_source_remover_real_cancel (SeahorseOperation* base);
static GCancellable* seahorse_pkcs11_source_remover_get_cancellable (SeahorsePkcs11SourceRemover* self);
static SeahorsePkcs11Source* seahorse_pkcs11_source_remover_get_source (SeahorsePkcs11SourceRemover* self);
static void seahorse_pkcs11_source_remover_set_source (SeahorsePkcs11SourceRemover* self, SeahorsePkcs11Source* value);
static SeahorsePkcs11Certificate* seahorse_pkcs11_source_remover_get_certificate (SeahorsePkcs11SourceRemover* self);
static void seahorse_pkcs11_source_remover_set_certificate (SeahorsePkcs11SourceRemover* self, SeahorsePkcs11Certificate* value);
static void _seahorse_pkcs11_source_remover_on_destroy_object_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self);
static GObject * seahorse_pkcs11_source_remover_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties);
static gpointer seahorse_pkcs11_source_remover_parent_class = NULL;
static void seahorse_pkcs11_source_remover_dispose (GObject * obj);
static GType seahorse_pkcs11_source_remover_get_type (void);
static gpointer seahorse_pkcs11_source_parent_class = NULL;
static void seahorse_pkcs11_source_dispose (GObject * obj);

static const guint SEAHORSE_PKCS11_ATTRIBUTE_TYPES[] = {CKA_LABEL, CKA_ID, CKA_CLASS, CKA_TOKEN, CKA_GNOME_USER_TRUST, CKA_START_DATE, CKA_END_DATE};


/* ---------------------------------------------------------------------------------
 * PUBLIC STUFF
 */
SeahorsePkcs11Source* seahorse_pkcs11_source_new (GP11Slot* slot) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorsePkcs11Source * self;
	g_return_val_if_fail (GP11_IS_SLOT (slot), NULL);
	__params = g_new0 (GParameter, 1);
	__params_it = __params;
	__params_it->name = "slot";
	g_value_init (&__params_it->value, GP11_TYPE_SLOT);
	g_value_set_object (&__params_it->value, slot);
	__params_it++;
	self = g_object_newv (SEAHORSE_PKCS11_TYPE_SOURCE, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


/* ---------------------------------------------------------------------------------
 * VIRTUAL METHODS
 */
static SeahorseOperation* seahorse_pkcs11_source_real_load (SeahorseSource* base, GQuark id) {
	SeahorsePkcs11Source * self;
	GP11Attributes* attrs;
	self = SEAHORSE_PKCS11_SOURCE (base);
	/* Load all objects */
	if (id == 0) {
		return SEAHORSE_OPERATION (seahorse_pkcs11_source_refresher_new (self));
	}
	/* Load only objects described by the id */
	attrs = gp11_attributes_new ();
	if (!seahorse_pkcs11_id_to_attributes (id, attrs)) {
		SeahorseOperation* _tmp1;
		_tmp1 = NULL;
		return (_tmp1 = seahorse_operation_new_complete (g_error_new (SEAHORSE_ERROR, 0, "%s", _ ("Invalid or unrecognized object."), NULL)), (attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL))), _tmp1);
	} else {
		SeahorseOperation* _tmp2;
		_tmp2 = NULL;
		return (_tmp2 = SEAHORSE_OPERATION (seahorse_pkcs11_source_loader_new (self, attrs)), (attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL))), _tmp2);
	}
	(attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL)));
}


static SeahorseOperation* seahorse_pkcs11_source_real_import (SeahorsePkcs11Source* self, GInputStream* input) {
	guchar* _tmp1;
	gint data_length1;
	gint _tmp0;
	guchar* data;
	GP11Attributes* attrs;
	SeahorseOperation* _tmp2;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self), NULL);
	g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
	_tmp1 = NULL;
	data = (_tmp1 = seahorse_util_read_to_memory (input, &_tmp0), data_length1 = _tmp0, _tmp1);
	attrs = gp11_attributes_new ();
	gp11_attributes_add_boolean (attrs, CKA_GNOME_IMPORT_TOKEN, TRUE);
	gp11_attributes_add_data (attrs, CKA_VALUE, data, data_length1);
	_tmp2 = NULL;
	return (_tmp2 = SEAHORSE_OPERATION (seahorse_pkcs11_source_importer_new (self, attrs)), (data = (g_free (data), NULL)), (attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL))), _tmp2);
}


SeahorseOperation* seahorse_pkcs11_source_import (SeahorsePkcs11Source* self, GInputStream* input) {
	return SEAHORSE_PKCS11_SOURCE_GET_CLASS (self)->import (self, input);
}


static SeahorseOperation* seahorse_pkcs11_source_real_export (SeahorsePkcs11Source* self, GList* objects, GOutputStream* output) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self), NULL);
	g_return_val_if_fail (objects != NULL, NULL);
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);
	return seahorse_operation_new_complete (g_error_new (SEAHORSE_ERROR, 0, "%s", _ ("Exporting is not yet supported."), NULL));
}


SeahorseOperation* seahorse_pkcs11_source_export (SeahorsePkcs11Source* self, GList* objects, GOutputStream* output) {
	return SEAHORSE_PKCS11_SOURCE_GET_CLASS (self)->export (self, objects, output);
}


static SeahorseOperation* seahorse_pkcs11_source_real_remove (SeahorsePkcs11Source* self, SeahorseObject* object) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self), NULL);
	g_return_val_if_fail (SEAHORSE_IS_OBJECT (object), NULL);
	return SEAHORSE_OPERATION (seahorse_pkcs11_source_remover_new (SEAHORSE_PKCS11_CERTIFICATE (object)));
}


SeahorseOperation* seahorse_pkcs11_source_remove (SeahorsePkcs11Source* self, SeahorseObject* object) {
	return SEAHORSE_PKCS11_SOURCE_GET_CLASS (self)->remove (self, object);
}


/* --------------------------------------------------------------------------------
 * HELPER METHODS
 */
static void seahorse_pkcs11_source_receive_object (SeahorsePkcs11Source* self, GP11Object* object, GP11Attributes* attrs) {
	GQuark id;
	gboolean created;
	SeahorsePkcs11Certificate* cert;
	SeahorseObject* _tmp0;
	SeahorseObject* prev;
	SeahorsePkcs11Certificate* _tmp3;
	g_return_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self));
	g_return_if_fail (GP11_IS_OBJECT (object));
	g_return_if_fail (attrs != NULL);
	/* Build up an identifier for this object */
	id = seahorse_pkcs11_id_from_attributes (attrs);
	g_return_if_fail (id != 0);
	created = FALSE;
	cert = NULL;
	/* Look for an already present object */
	_tmp0 = NULL;
	prev = (_tmp0 = seahorse_context_get_object (seahorse_context_for_app (), SEAHORSE_SOURCE (self), id), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	if (prev != NULL && G_TYPE_FROM_INSTANCE (G_OBJECT (prev)) == SEAHORSE_PKCS11_TYPE_CERTIFICATE) {
		SeahorsePkcs11Certificate* _tmp2;
		SeahorsePkcs11Certificate* _tmp1;
		_tmp2 = NULL;
		_tmp1 = NULL;
		cert = (_tmp2 = (_tmp1 = SEAHORSE_PKCS11_CERTIFICATE (prev), (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))), (cert == NULL ? NULL : (cert = (g_object_unref (cert), NULL))), _tmp2);
		seahorse_pkcs11_certificate_set_pkcs11_object (cert, object);
		seahorse_pkcs11_certificate_set_pkcs11_attributes (cert, attrs);
		(cert == NULL ? NULL : (cert = (g_object_unref (cert), NULL)));
		(prev == NULL ? NULL : (prev = (g_object_unref (prev), NULL)));
		return;
	}
	/* Create a new object */
	_tmp3 = NULL;
	cert = (_tmp3 = seahorse_pkcs11_certificate_new (object, attrs), (cert == NULL ? NULL : (cert = (g_object_unref (cert), NULL))), _tmp3);
	seahorse_context_add_object (seahorse_context_for_app (), SEAHORSE_OBJECT (cert));
	(cert == NULL ? NULL : (cert = (g_object_unref (cert), NULL)));
	(prev == NULL ? NULL : (prev = (g_object_unref (prev), NULL)));
}


GP11Slot* seahorse_pkcs11_source_get_slot (SeahorsePkcs11Source* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self), NULL);
	return self->priv->_slot;
}


static void seahorse_pkcs11_source_set_slot (SeahorsePkcs11Source* self, GP11Slot* value) {
	GP11Slot* _tmp2;
	GP11Slot* _tmp1;
	g_return_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->priv->_slot = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))), (self->priv->_slot == NULL ? NULL : (self->priv->_slot = (g_object_unref (self->priv->_slot), NULL))), _tmp2);
	g_object_notify (((GObject *) (self)), "slot");
}


SeahorseLocation seahorse_pkcs11_source_get_location (SeahorsePkcs11Source* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self), 0);
	return SEAHORSE_LOCATION_LOCAL;
}


GQuark seahorse_pkcs11_source_get_key_type (SeahorsePkcs11Source* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self), 0U);
	return SEAHORSE_PKCS11_TYPE;
}


const char* seahorse_pkcs11_source_get_key_desc (SeahorsePkcs11Source* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (self), NULL);
	return _ ("X509 Certificate");
}


static void _g_list_free_g_object_unref (GList* self) {
	g_list_foreach (self, ((GFunc) (g_object_unref)), NULL);
	g_list_free (self);
}


static void seahorse_pkcs11_source_updater_complete (SeahorsePkcs11SourceUpdater* self, GError* err) {
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self));
	if (err == NULL) {
		seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, NULL);
	} else {
		if (err->code == CKR_FUNCTION_CANCELED) {
			seahorse_operation_mark_done (SEAHORSE_OPERATION (self), TRUE, NULL);
		} else {
			GError* _tmp0;
			_tmp0 = NULL;
			seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, (_tmp0 = err, (_tmp0 == NULL || g_error_copy == NULL ? ((gpointer) (_tmp0)) : g_error_copy (((gpointer) (_tmp0))))));
		}
	}
}


static void seahorse_pkcs11_source_updater_on_open_session (SeahorsePkcs11SourceUpdater* self, GObject* obj, GAsyncResult* result) {
	GError * inner_error;
	GP11Slot* _tmp0;
	GP11Slot* slot;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self));
	g_return_if_fail (G_IS_OBJECT (obj));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	inner_error = NULL;
	_tmp0 = NULL;
	slot = (_tmp0 = GP11_SLOT (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	{
		GP11Session* _tmp1;
		_tmp1 = NULL;
		self->_session = (_tmp1 = gp11_slot_open_session_finish (slot, result, &inner_error), (self->_session == NULL ? NULL : (self->_session = (g_object_unref (self->_session), NULL))), _tmp1);
		if (inner_error != NULL) {
			goto __catch0_g_error;
		}
	}
	goto __finally0;
	__catch0_g_error:
	{
		GError * err;
		err = inner_error;
		inner_error = NULL;
		{
			seahorse_pkcs11_source_updater_complete (self, err);
			(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
			(slot == NULL ? NULL : (slot = (g_object_unref (slot), NULL)));
			return;
		}
	}
	__finally0:
	;
	/* Step 2. Load all the objects that we want */
	seahorse_pkcs11_source_updater_load_objects (self);
	(slot == NULL ? NULL : (slot = (g_object_unref (slot), NULL)));
}


static void seahorse_pkcs11_source_updater_real_load_objects (SeahorsePkcs11SourceUpdater* self) {
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self));
	g_critical ("Type `%s' does not implement abstract method `seahorse_pkcs11_source_updater_load_objects'", g_type_name (G_TYPE_FROM_INSTANCE (self)));
	return;
}


static void seahorse_pkcs11_source_updater_load_objects (SeahorsePkcs11SourceUpdater* self) {
	SEAHORSE_PKCS11_SOURCE_UPDATER_GET_CLASS (self)->load_objects (self);
}


static void seahorse_pkcs11_source_updater_loaded_objects (SeahorsePkcs11SourceUpdater* self, GList* objects) {
	GList* _tmp0;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self));
	g_return_if_fail (objects != NULL);
	_tmp0 = NULL;
	self->priv->_objects = (_tmp0 = NULL, (self->priv->_objects == NULL ? NULL : (self->priv->_objects = (_g_list_free_g_object_unref (self->priv->_objects), NULL))), _tmp0);
	{
		GList* object_collection;
		GList* object_it;
		object_collection = objects;
		for (object_it = object_collection; object_it != NULL; object_it = object_it->next) {
			GP11Object* _tmp2;
			GP11Object* object;
			_tmp2 = NULL;
			object = (_tmp2 = ((GP11Object*) (object_it->data)), (_tmp2 == NULL ? NULL : g_object_ref (_tmp2)));
			{
				GP11Object* _tmp1;
				_tmp1 = NULL;
				self->priv->_objects = g_list_append (self->priv->_objects, (_tmp1 = object, (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))));
				(object == NULL ? NULL : (object = (g_object_unref (object), NULL)));
			}
		}
	}
	self->priv->_total = ((gint) (g_list_length (self->priv->_objects)));
	/* Step 3. Load information for each object */
	seahorse_pkcs11_source_updater_do_get_attributes (self);
}


static void _seahorse_pkcs11_source_updater_on_get_attributes_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self) {
	seahorse_pkcs11_source_updater_on_get_attributes (self, source_object, res);
}


static void seahorse_pkcs11_source_updater_do_get_attributes (SeahorsePkcs11SourceUpdater* self) {
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self));
	/* No more objects to load? */
	if (g_list_length (self->priv->_objects) == 0) {
		seahorse_pkcs11_source_updater_complete (self, NULL);
	} else {
		GP11Object* _tmp0;
		GP11Object* object;
		/* Load the next object */
		seahorse_operation_mark_progress_full (SEAHORSE_OPERATION (self), _ ("Loading..."), ((gint) (g_list_length (self->priv->_objects))) - self->priv->_total, self->priv->_total);
		_tmp0 = NULL;
		object = (_tmp0 = ((GP11Object*) (((GP11Object*) (self->priv->_objects->data)))), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
		self->priv->_objects = g_list_remove (self->priv->_objects, object);
		gp11_object_get_async (object, SEAHORSE_PKCS11_ATTRIBUTE_TYPES, G_N_ELEMENTS (SEAHORSE_PKCS11_ATTRIBUTE_TYPES), self->priv->_cancellable, _seahorse_pkcs11_source_updater_on_get_attributes_gasync_ready_callback, self);
		(object == NULL ? NULL : (object = (g_object_unref (object), NULL)));
	}
}


static void seahorse_pkcs11_source_updater_on_get_attributes (SeahorsePkcs11SourceUpdater* self, GObject* obj, GAsyncResult* result) {
	GError * inner_error;
	GP11Object* _tmp0;
	GP11Object* object;
	GP11Attributes* attrs;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self));
	g_return_if_fail (G_IS_OBJECT (obj));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	inner_error = NULL;
	_tmp0 = NULL;
	object = (_tmp0 = GP11_OBJECT (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	attrs = NULL;
	{
		GP11Attributes* _tmp1;
		_tmp1 = NULL;
		attrs = (_tmp1 = gp11_object_get_finish (object, result, &inner_error), (attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL))), _tmp1);
		if (inner_error != NULL) {
			goto __catch1_g_error;
		}
	}
	goto __finally1;
	__catch1_g_error:
	{
		GError * err;
		err = inner_error;
		inner_error = NULL;
		{
			/* Ignore objects that have gone away */
			if (err->code != CKR_OBJECT_HANDLE_INVALID) {
				seahorse_pkcs11_source_updater_complete (self, err);
				(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
				(object == NULL ? NULL : (object = (g_object_unref (object), NULL)));
				(attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL)));
				return;
			}
			(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
		}
	}
	__finally1:
	;
	/* Process this object */
	seahorse_pkcs11_source_receive_object (self->_source, object, attrs);
	/* Do the next object */
	seahorse_pkcs11_source_updater_do_get_attributes (self);
	(object == NULL ? NULL : (object = (g_object_unref (object), NULL)));
	(attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL)));
}


/* Cancel the operation */
static void seahorse_pkcs11_source_updater_real_cancel (SeahorseOperation* base) {
	SeahorsePkcs11SourceUpdater * self;
	self = SEAHORSE_PKCS11_SOURCE_UPDATER (base);
	g_cancellable_cancel (self->priv->_cancellable);
}


static GCancellable* seahorse_pkcs11_source_updater_get_cancellable (SeahorsePkcs11SourceUpdater* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self), NULL);
	return self->priv->_cancellable;
}


static SeahorsePkcs11Source* seahorse_pkcs11_source_updater_get_source (SeahorsePkcs11SourceUpdater* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self), NULL);
	return self->_source;
}


static void seahorse_pkcs11_source_updater_set_source (SeahorsePkcs11SourceUpdater* self, SeahorsePkcs11Source* value) {
	SeahorsePkcs11Source* _tmp2;
	SeahorsePkcs11Source* _tmp1;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_UPDATER (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->_source = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))), (self->_source == NULL ? NULL : (self->_source = (g_object_unref (self->_source), NULL))), _tmp2);
	g_object_notify (((GObject *) (self)), "source");
}


static void _seahorse_pkcs11_source_updater_on_open_session_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self) {
	seahorse_pkcs11_source_updater_on_open_session (self, source_object, res);
}


static GObject * seahorse_pkcs11_source_updater_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorsePkcs11SourceUpdaterClass * klass;
	GObjectClass * parent_class;
	SeahorsePkcs11SourceUpdater * self;
	klass = SEAHORSE_PKCS11_SOURCE_UPDATER_CLASS (g_type_class_peek (SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_PKCS11_SOURCE_UPDATER (obj);
	{
		GCancellable* _tmp0;
		GP11Session* _tmp1;
		_tmp0 = NULL;
		self->priv->_cancellable = (_tmp0 = g_cancellable_new (), (self->priv->_cancellable == NULL ? NULL : (self->priv->_cancellable = (g_object_unref (self->priv->_cancellable), NULL))), _tmp0);
		_tmp1 = NULL;
		self->_session = (_tmp1 = NULL, (self->_session == NULL ? NULL : (self->_session = (g_object_unref (self->_session), NULL))), _tmp1);
		/* Step 1. Load the session */
		gp11_slot_open_session_async (seahorse_pkcs11_source_get_slot (self->_source), CKF_RW_SESSION, self->priv->_cancellable, _seahorse_pkcs11_source_updater_on_open_session_gasync_ready_callback, self);
		seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	}
	return obj;
}


static void seahorse_pkcs11_source_updater_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11SourceUpdater * self;
	self = SEAHORSE_PKCS11_SOURCE_UPDATER (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_UPDATER_CANCELLABLE:
		g_value_set_object (value, seahorse_pkcs11_source_updater_get_cancellable (self));
		break;
		case SEAHORSE_PKCS11_SOURCE_UPDATER_SOURCE:
		g_value_set_object (value, seahorse_pkcs11_source_updater_get_source (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_updater_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11SourceUpdater * self;
	self = SEAHORSE_PKCS11_SOURCE_UPDATER (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_UPDATER_SOURCE:
		seahorse_pkcs11_source_updater_set_source (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_updater_class_init (SeahorsePkcs11SourceUpdaterClass * klass) {
	seahorse_pkcs11_source_updater_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11SourceUpdaterPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pkcs11_source_updater_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_pkcs11_source_updater_set_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_pkcs11_source_updater_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pkcs11_source_updater_dispose;
	SEAHORSE_PKCS11_SOURCE_UPDATER_CLASS (klass)->load_objects = seahorse_pkcs11_source_updater_real_load_objects;
	SEAHORSE_OPERATION_CLASS (klass)->cancel = seahorse_pkcs11_source_updater_real_cancel;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_UPDATER_CANCELLABLE, g_param_spec_object ("cancellable", "cancellable", "cancellable", G_TYPE_CANCELLABLE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_UPDATER_SOURCE, g_param_spec_object ("source", "source", "source", SEAHORSE_PKCS11_TYPE_SOURCE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}


static void seahorse_pkcs11_source_updater_instance_init (SeahorsePkcs11SourceUpdater * self) {
	self->priv = SEAHORSE_PKCS11_SOURCE_UPDATER_GET_PRIVATE (self);
}


static void seahorse_pkcs11_source_updater_dispose (GObject * obj) {
	SeahorsePkcs11SourceUpdater * self;
	self = SEAHORSE_PKCS11_SOURCE_UPDATER (obj);
	(self->priv->_cancellable == NULL ? NULL : (self->priv->_cancellable = (g_object_unref (self->priv->_cancellable), NULL)));
	(self->_source == NULL ? NULL : (self->_source = (g_object_unref (self->_source), NULL)));
	(self->_session == NULL ? NULL : (self->_session = (g_object_unref (self->_session), NULL)));
	(self->priv->_objects == NULL ? NULL : (self->priv->_objects = (_g_list_free_g_object_unref (self->priv->_objects), NULL)));
	G_OBJECT_CLASS (seahorse_pkcs11_source_updater_parent_class)->dispose (obj);
}


static GType seahorse_pkcs11_source_updater_get_type (void) {
	static GType seahorse_pkcs11_source_updater_type_id = 0;
	if (G_UNLIKELY (seahorse_pkcs11_source_updater_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePkcs11SourceUpdaterClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pkcs11_source_updater_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePkcs11SourceUpdater), 0, (GInstanceInitFunc) seahorse_pkcs11_source_updater_instance_init };
		seahorse_pkcs11_source_updater_type_id = g_type_register_static (SEAHORSE_TYPE_OPERATION, "SeahorsePkcs11SourceUpdater", &g_define_type_info, G_TYPE_FLAG_ABSTRACT);
	}
	return seahorse_pkcs11_source_updater_type_id;
}


static SeahorsePkcs11SourceRefresher* seahorse_pkcs11_source_refresher_new (SeahorsePkcs11Source* source) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorsePkcs11SourceRefresher * self;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (source), NULL);
	__params = g_new0 (GParameter, 1);
	__params_it = __params;
	__params_it->name = "source";
	g_value_init (&__params_it->value, SEAHORSE_PKCS11_TYPE_SOURCE);
	g_value_set_object (&__params_it->value, source);
	__params_it++;
	self = g_object_newv (SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


static void _seahorse_pkcs11_source_refresher_on_find_objects_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self) {
	seahorse_pkcs11_source_refresher_on_find_objects (self, source_object, res);
}


/* Search for all token objects */
static void seahorse_pkcs11_source_refresher_real_load_objects (SeahorsePkcs11SourceUpdater* base) {
	SeahorsePkcs11SourceRefresher * self;
	GP11Attributes* attrs;
	self = SEAHORSE_PKCS11_SOURCE_REFRESHER (base);
	attrs = gp11_attributes_new ();
	gp11_attributes_add_boolean (attrs, CKA_TOKEN, TRUE);
	gp11_attributes_add_ulong (attrs, CKA_CLASS, CKO_CERTIFICATE);
	gp11_session_find_objects_async (SEAHORSE_PKCS11_SOURCE_UPDATER (self)->_session, attrs, seahorse_pkcs11_source_updater_get_cancellable (SEAHORSE_PKCS11_SOURCE_UPDATER (self)), _seahorse_pkcs11_source_refresher_on_find_objects_gasync_ready_callback, self);
	(attrs == NULL ? NULL : (attrs = (gp11_attributes_unref (attrs), NULL)));
}


static void __lambda0 (void* k, void* v, SeahorsePkcs11SourceRefresher* self) {
	seahorse_context_remove_object (seahorse_context_for_app (), SEAHORSE_OBJECT (v));
}


static void ___lambda0_gh_func (void* key, void* value, gpointer self) {
	__lambda0 (key, value, self);
}


static void seahorse_pkcs11_source_refresher_on_find_objects (SeahorsePkcs11SourceRefresher* self, GObject* obj, GAsyncResult* result) {
	GError * inner_error;
	GP11Session* _tmp0;
	GP11Session* session;
	GList* objects;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_REFRESHER (self));
	g_return_if_fail (G_IS_OBJECT (obj));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	inner_error = NULL;
	_tmp0 = NULL;
	session = (_tmp0 = GP11_SESSION (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	objects = NULL;
	{
		GList* _tmp1;
		_tmp1 = NULL;
		objects = (_tmp1 = gp11_session_find_objects_finish (session, result, &inner_error), (objects == NULL ? NULL : (objects = (_g_list_free_g_object_unref (objects), NULL))), _tmp1);
		if (inner_error != NULL) {
			goto __catch2_g_error;
		}
	}
	goto __finally2;
	__catch2_g_error:
	{
		GError * err;
		err = inner_error;
		inner_error = NULL;
		{
			seahorse_pkcs11_source_updater_complete (SEAHORSE_PKCS11_SOURCE_UPDATER (self), err);
			(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
			(session == NULL ? NULL : (session = (g_object_unref (session), NULL)));
			(objects == NULL ? NULL : (objects = (_g_list_free_g_object_unref (objects), NULL)));
			return;
		}
	}
	__finally2:
	;
	/* Remove all objects that were found, from the check table */
	{
		GList* object_collection;
		GList* object_it;
		object_collection = objects;
		for (object_it = object_collection; object_it != NULL; object_it = object_it->next) {
			GP11Object* _tmp2;
			GP11Object* object;
			_tmp2 = NULL;
			object = (_tmp2 = ((GP11Object*) (object_it->data)), (_tmp2 == NULL ? NULL : g_object_ref (_tmp2)));
			{
				g_hash_table_remove (self->priv->_checks, GUINT_TO_POINTER (gp11_object_get_handle (object)));
				(object == NULL ? NULL : (object = (g_object_unref (object), NULL)));
			}
		}
	}
	g_hash_table_foreach (self->priv->_checks, ___lambda0_gh_func, self);
	seahorse_pkcs11_source_updater_loaded_objects (SEAHORSE_PKCS11_SOURCE_UPDATER (self), objects);
	(session == NULL ? NULL : (session = (g_object_unref (session), NULL)));
	(objects == NULL ? NULL : (objects = (_g_list_free_g_object_unref (objects), NULL)));
}


static GObject * seahorse_pkcs11_source_refresher_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorsePkcs11SourceRefresherClass * klass;
	GObjectClass * parent_class;
	SeahorsePkcs11SourceRefresher * self;
	klass = SEAHORSE_PKCS11_SOURCE_REFRESHER_CLASS (g_type_class_peek (SEAHORSE_PKCS11_SOURCE_TYPE_REFRESHER));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_PKCS11_SOURCE_REFRESHER (obj);
	{
		GHashTable* _tmp2;
		/* Load all the current item into the check table */
		_tmp2 = NULL;
		self->priv->_checks = (_tmp2 = g_hash_table_new (g_direct_hash, g_direct_equal), (self->priv->_checks == NULL ? NULL : (self->priv->_checks = (g_hash_table_unref (self->priv->_checks), NULL))), _tmp2);
		{
			GList* object_collection;
			GList* object_it;
			object_collection = seahorse_context_get_objects (seahorse_context_for_app (), SEAHORSE_SOURCE (SEAHORSE_PKCS11_SOURCE_UPDATER (self)->_source));
			for (object_it = object_collection; object_it != NULL; object_it = object_it->next) {
				SeahorseObject* object;
				object = ((SeahorseObject*) (object_it->data));
				{
					if (G_TYPE_FROM_INSTANCE (G_OBJECT (object)) == SEAHORSE_PKCS11_TYPE_CERTIFICATE) {
						SeahorsePkcs11Certificate* _tmp3;
						SeahorsePkcs11Certificate* certificate;
						_tmp3 = NULL;
						certificate = (_tmp3 = SEAHORSE_PKCS11_CERTIFICATE (object), (_tmp3 == NULL ? NULL : g_object_ref (_tmp3)));
						if (seahorse_pkcs11_certificate_get_pkcs11_object (certificate) != NULL) {
							SeahorseObject* _tmp4;
							_tmp4 = NULL;
							g_hash_table_insert (self->priv->_checks, GUINT_TO_POINTER (gp11_object_get_handle (seahorse_pkcs11_certificate_get_pkcs11_object (certificate))), (_tmp4 = object, (_tmp4 == NULL ? NULL : g_object_ref (_tmp4))));
						}
						(certificate == NULL ? NULL : (certificate = (g_object_unref (certificate), NULL)));
					}
				}
			}
			(object_collection == NULL ? NULL : (object_collection = (g_list_free (object_collection), NULL)));
		}
	}
	return obj;
}


static void seahorse_pkcs11_source_refresher_class_init (SeahorsePkcs11SourceRefresherClass * klass) {
	seahorse_pkcs11_source_refresher_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11SourceRefresherPrivate));
	G_OBJECT_CLASS (klass)->constructor = seahorse_pkcs11_source_refresher_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pkcs11_source_refresher_dispose;
	SEAHORSE_PKCS11_SOURCE_UPDATER_CLASS (klass)->load_objects = seahorse_pkcs11_source_refresher_real_load_objects;
}


static void seahorse_pkcs11_source_refresher_instance_init (SeahorsePkcs11SourceRefresher * self) {
	self->priv = SEAHORSE_PKCS11_SOURCE_REFRESHER_GET_PRIVATE (self);
}


static void seahorse_pkcs11_source_refresher_dispose (GObject * obj) {
	SeahorsePkcs11SourceRefresher * self;
	self = SEAHORSE_PKCS11_SOURCE_REFRESHER (obj);
	(self->priv->_checks == NULL ? NULL : (self->priv->_checks = (g_hash_table_unref (self->priv->_checks), NULL)));
	G_OBJECT_CLASS (seahorse_pkcs11_source_refresher_parent_class)->dispose (obj);
}


static GType seahorse_pkcs11_source_refresher_get_type (void) {
	static GType seahorse_pkcs11_source_refresher_type_id = 0;
	if (G_UNLIKELY (seahorse_pkcs11_source_refresher_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePkcs11SourceRefresherClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pkcs11_source_refresher_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePkcs11SourceRefresher), 0, (GInstanceInitFunc) seahorse_pkcs11_source_refresher_instance_init };
		seahorse_pkcs11_source_refresher_type_id = g_type_register_static (SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER, "SeahorsePkcs11SourceRefresher", &g_define_type_info, 0);
	}
	return seahorse_pkcs11_source_refresher_type_id;
}


static SeahorsePkcs11SourceLoader* seahorse_pkcs11_source_loader_new (SeahorsePkcs11Source* source, GP11Attributes* unique_attrs) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorsePkcs11SourceLoader * self;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (source), NULL);
	g_return_val_if_fail (unique_attrs != NULL, NULL);
	__params = g_new0 (GParameter, 2);
	__params_it = __params;
	__params_it->name = "source";
	g_value_init (&__params_it->value, SEAHORSE_PKCS11_TYPE_SOURCE);
	g_value_set_object (&__params_it->value, source);
	__params_it++;
	__params_it->name = "unique-attrs";
	g_value_init (&__params_it->value, GP11_TYPE_ATTRIBUTES);
	g_value_set_pointer (&__params_it->value, unique_attrs);
	__params_it++;
	self = g_object_newv (SEAHORSE_PKCS11_SOURCE_TYPE_LOADER, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


static void _seahorse_pkcs11_source_loader_on_find_objects_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self) {
	seahorse_pkcs11_source_loader_on_find_objects (self, source_object, res);
}


/* Search for the object to load */
static void seahorse_pkcs11_source_loader_real_load_objects (SeahorsePkcs11SourceUpdater* base) {
	SeahorsePkcs11SourceLoader * self;
	self = SEAHORSE_PKCS11_SOURCE_LOADER (base);
	gp11_session_find_objects_async (SEAHORSE_PKCS11_SOURCE_UPDATER (self)->_session, self->priv->_unique_attrs, seahorse_pkcs11_source_updater_get_cancellable (SEAHORSE_PKCS11_SOURCE_UPDATER (self)), _seahorse_pkcs11_source_loader_on_find_objects_gasync_ready_callback, self);
}


static void seahorse_pkcs11_source_loader_on_find_objects (SeahorsePkcs11SourceLoader* self, GObject* obj, GAsyncResult* result) {
	GError * inner_error;
	GP11Session* _tmp0;
	GP11Session* session;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_LOADER (self));
	g_return_if_fail (G_IS_OBJECT (obj));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	inner_error = NULL;
	_tmp0 = NULL;
	session = (_tmp0 = GP11_SESSION (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	{
		GList* objects;
		objects = gp11_session_find_objects_finish (session, result, &inner_error);
		if (inner_error != NULL) {
			goto __catch3_g_error;
		}
		seahorse_pkcs11_source_updater_loaded_objects (SEAHORSE_PKCS11_SOURCE_UPDATER (self), objects);
		(objects == NULL ? NULL : (objects = (_g_list_free_g_object_unref (objects), NULL)));
	}
	goto __finally3;
	__catch3_g_error:
	{
		GError * err;
		err = inner_error;
		inner_error = NULL;
		{
			seahorse_pkcs11_source_updater_complete (SEAHORSE_PKCS11_SOURCE_UPDATER (self), err);
			(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
			(session == NULL ? NULL : (session = (g_object_unref (session), NULL)));
			return;
		}
	}
	__finally3:
	;
	(session == NULL ? NULL : (session = (g_object_unref (session), NULL)));
}


static GP11Attributes* seahorse_pkcs11_source_loader_get_unique_attrs (SeahorsePkcs11SourceLoader* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_SOURCE_IS_LOADER (self), NULL);
	return self->priv->_unique_attrs;
}


static void seahorse_pkcs11_source_loader_set_unique_attrs (SeahorsePkcs11SourceLoader* self, GP11Attributes* value) {
	GP11Attributes* _tmp2;
	GP11Attributes* _tmp1;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_LOADER (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->priv->_unique_attrs = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : gp11_attributes_ref (_tmp1))), (self->priv->_unique_attrs == NULL ? NULL : (self->priv->_unique_attrs = (gp11_attributes_unref (self->priv->_unique_attrs), NULL))), _tmp2);
	g_object_notify (((GObject *) (self)), "unique-attrs");
}


static void seahorse_pkcs11_source_loader_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11SourceLoader * self;
	self = SEAHORSE_PKCS11_SOURCE_LOADER (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_LOADER_UNIQUE_ATTRS:
		g_value_set_pointer (value, seahorse_pkcs11_source_loader_get_unique_attrs (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_loader_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11SourceLoader * self;
	self = SEAHORSE_PKCS11_SOURCE_LOADER (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_LOADER_UNIQUE_ATTRS:
		seahorse_pkcs11_source_loader_set_unique_attrs (self, g_value_get_pointer (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_loader_class_init (SeahorsePkcs11SourceLoaderClass * klass) {
	seahorse_pkcs11_source_loader_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11SourceLoaderPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pkcs11_source_loader_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_pkcs11_source_loader_set_property;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pkcs11_source_loader_dispose;
	SEAHORSE_PKCS11_SOURCE_UPDATER_CLASS (klass)->load_objects = seahorse_pkcs11_source_loader_real_load_objects;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_LOADER_UNIQUE_ATTRS, g_param_spec_pointer ("unique-attrs", "unique-attrs", "unique-attrs", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}


static void seahorse_pkcs11_source_loader_instance_init (SeahorsePkcs11SourceLoader * self) {
	self->priv = SEAHORSE_PKCS11_SOURCE_LOADER_GET_PRIVATE (self);
}


static void seahorse_pkcs11_source_loader_dispose (GObject * obj) {
	SeahorsePkcs11SourceLoader * self;
	self = SEAHORSE_PKCS11_SOURCE_LOADER (obj);
	(self->priv->_unique_attrs == NULL ? NULL : (self->priv->_unique_attrs = (gp11_attributes_unref (self->priv->_unique_attrs), NULL)));
	G_OBJECT_CLASS (seahorse_pkcs11_source_loader_parent_class)->dispose (obj);
}


static GType seahorse_pkcs11_source_loader_get_type (void) {
	static GType seahorse_pkcs11_source_loader_type_id = 0;
	if (G_UNLIKELY (seahorse_pkcs11_source_loader_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePkcs11SourceLoaderClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pkcs11_source_loader_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePkcs11SourceLoader), 0, (GInstanceInitFunc) seahorse_pkcs11_source_loader_instance_init };
		seahorse_pkcs11_source_loader_type_id = g_type_register_static (SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER, "SeahorsePkcs11SourceLoader", &g_define_type_info, 0);
	}
	return seahorse_pkcs11_source_loader_type_id;
}


static SeahorsePkcs11SourceImporter* seahorse_pkcs11_source_importer_new (SeahorsePkcs11Source* source, GP11Attributes* import) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorsePkcs11SourceImporter * self;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_SOURCE (source), NULL);
	g_return_val_if_fail (import != NULL, NULL);
	__params = g_new0 (GParameter, 1);
	__params_it = __params;
	__params_it->name = "source";
	g_value_init (&__params_it->value, SEAHORSE_PKCS11_TYPE_SOURCE);
	g_value_set_object (&__params_it->value, source);
	__params_it++;
	self = g_object_newv (SEAHORSE_PKCS11_SOURCE_TYPE_IMPORTER, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


static void _seahorse_pkcs11_source_importer_on_create_object_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self) {
	seahorse_pkcs11_source_importer_on_create_object (self, source_object, res);
}


/* Search for the object to load */
static void seahorse_pkcs11_source_importer_real_load_objects (SeahorsePkcs11SourceUpdater* base) {
	SeahorsePkcs11SourceImporter * self;
	self = SEAHORSE_PKCS11_SOURCE_IMPORTER (base);
	gp11_session_create_object_async (SEAHORSE_PKCS11_SOURCE_UPDATER (self)->_session, self->priv->_import_data, seahorse_pkcs11_source_updater_get_cancellable (SEAHORSE_PKCS11_SOURCE_UPDATER (self)), _seahorse_pkcs11_source_importer_on_create_object_gasync_ready_callback, self);
}


static void _seahorse_pkcs11_source_importer_on_get_attribute_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self) {
	seahorse_pkcs11_source_importer_on_get_attribute (self, source_object, res);
}


static void seahorse_pkcs11_source_importer_on_create_object (SeahorsePkcs11SourceImporter* self, GObject* obj, GAsyncResult* result) {
	GError * inner_error;
	GP11Session* _tmp0;
	GP11Session* session;
	GP11Object* import;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_IMPORTER (self));
	g_return_if_fail (G_IS_OBJECT (obj));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	inner_error = NULL;
	_tmp0 = NULL;
	session = (_tmp0 = GP11_SESSION (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	import = NULL;
	{
		GP11Object* _tmp1;
		_tmp1 = NULL;
		import = (_tmp1 = gp11_session_create_object_finish (session, result, &inner_error), (import == NULL ? NULL : (import = (g_object_unref (import), NULL))), _tmp1);
		if (inner_error != NULL) {
			goto __catch4_g_error;
		}
	}
	goto __finally4;
	__catch4_g_error:
	{
		GError * err;
		err = inner_error;
		inner_error = NULL;
		{
			seahorse_pkcs11_source_updater_complete (SEAHORSE_PKCS11_SOURCE_UPDATER (self), err);
			(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
			(session == NULL ? NULL : (session = (g_object_unref (session), NULL)));
			(import == NULL ? NULL : (import = (g_object_unref (import), NULL)));
			return;
		}
	}
	__finally4:
	;
	/* Get the list of objects imported */
	gp11_object_get_one_async (import, CKA_GNOME_IMPORT_OBJECTS, seahorse_pkcs11_source_updater_get_cancellable (SEAHORSE_PKCS11_SOURCE_UPDATER (self)), _seahorse_pkcs11_source_importer_on_get_attribute_gasync_ready_callback, self);
	(session == NULL ? NULL : (session = (g_object_unref (session), NULL)));
	(import == NULL ? NULL : (import = (g_object_unref (import), NULL)));
}


static void seahorse_pkcs11_source_importer_on_get_attribute (SeahorsePkcs11SourceImporter* self, GObject* obj, GAsyncResult* result) {
	GError * inner_error;
	GP11Object* _tmp0;
	GP11Object* import;
	GP11Attribute* imported_handles;
	GList* _tmp2;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_IMPORTER (self));
	g_return_if_fail (G_IS_OBJECT (obj));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	inner_error = NULL;
	_tmp0 = NULL;
	import = (_tmp0 = GP11_OBJECT (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	imported_handles = NULL;
	{
		GP11Attribute* _tmp1;
		_tmp1 = NULL;
		imported_handles = (_tmp1 = gp11_object_get_one_finish (import, result, &inner_error), (imported_handles == NULL ? NULL : (imported_handles = (gp11_attribute_free (imported_handles), NULL))), _tmp1);
		if (inner_error != NULL) {
			goto __catch5_g_error;
		}
	}
	goto __finally5;
	__catch5_g_error:
	{
		GError * err;
		err = inner_error;
		inner_error = NULL;
		{
			seahorse_pkcs11_source_updater_complete (SEAHORSE_PKCS11_SOURCE_UPDATER (self), err);
			(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
			(import == NULL ? NULL : (import = (g_object_unref (import), NULL)));
			(imported_handles == NULL ? NULL : (imported_handles = (gp11_attribute_free (imported_handles), NULL)));
			return;
		}
	}
	__finally5:
	;
	_tmp2 = NULL;
	seahorse_pkcs11_source_updater_loaded_objects (SEAHORSE_PKCS11_SOURCE_UPDATER (self), (_tmp2 = gp11_objects_from_handle_array (SEAHORSE_PKCS11_SOURCE_UPDATER (self)->_session, imported_handles)));
	(_tmp2 == NULL ? NULL : (_tmp2 = (_g_list_free_g_object_unref (_tmp2), NULL)));
	(import == NULL ? NULL : (import = (g_object_unref (import), NULL)));
	(imported_handles == NULL ? NULL : (imported_handles = (gp11_attribute_free (imported_handles), NULL)));
}


static GP11Attributes* seahorse_pkcs11_source_importer_get_import_data (SeahorsePkcs11SourceImporter* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_SOURCE_IS_IMPORTER (self), NULL);
	return self->priv->_import_data;
}


static void seahorse_pkcs11_source_importer_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11SourceImporter * self;
	self = SEAHORSE_PKCS11_SOURCE_IMPORTER (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_IMPORTER_IMPORT_DATA:
		g_value_set_pointer (value, seahorse_pkcs11_source_importer_get_import_data (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_importer_class_init (SeahorsePkcs11SourceImporterClass * klass) {
	seahorse_pkcs11_source_importer_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11SourceImporterPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pkcs11_source_importer_get_property;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pkcs11_source_importer_dispose;
	SEAHORSE_PKCS11_SOURCE_UPDATER_CLASS (klass)->load_objects = seahorse_pkcs11_source_importer_real_load_objects;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_IMPORTER_IMPORT_DATA, g_param_spec_pointer ("import-data", "import-data", "import-data", G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}


static void seahorse_pkcs11_source_importer_instance_init (SeahorsePkcs11SourceImporter * self) {
	self->priv = SEAHORSE_PKCS11_SOURCE_IMPORTER_GET_PRIVATE (self);
}


static void seahorse_pkcs11_source_importer_dispose (GObject * obj) {
	SeahorsePkcs11SourceImporter * self;
	self = SEAHORSE_PKCS11_SOURCE_IMPORTER (obj);
	(self->priv->_import_data == NULL ? NULL : (self->priv->_import_data = (gp11_attributes_unref (self->priv->_import_data), NULL)));
	G_OBJECT_CLASS (seahorse_pkcs11_source_importer_parent_class)->dispose (obj);
}


static GType seahorse_pkcs11_source_importer_get_type (void) {
	static GType seahorse_pkcs11_source_importer_type_id = 0;
	if (G_UNLIKELY (seahorse_pkcs11_source_importer_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePkcs11SourceImporterClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pkcs11_source_importer_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePkcs11SourceImporter), 0, (GInstanceInitFunc) seahorse_pkcs11_source_importer_instance_init };
		seahorse_pkcs11_source_importer_type_id = g_type_register_static (SEAHORSE_PKCS11_SOURCE_TYPE_UPDATER, "SeahorsePkcs11SourceImporter", &g_define_type_info, 0);
	}
	return seahorse_pkcs11_source_importer_type_id;
}


static SeahorsePkcs11SourceRemover* seahorse_pkcs11_source_remover_new (SeahorsePkcs11Certificate* certificate) {
	GParameter * __params;
	GParameter * __params_it;
	SeahorsePkcs11SourceRemover * self;
	g_return_val_if_fail (SEAHORSE_PKCS11_IS_CERTIFICATE (certificate), NULL);
	__params = g_new0 (GParameter, 1);
	__params_it = __params;
	__params_it->name = "certificate";
	g_value_init (&__params_it->value, SEAHORSE_PKCS11_TYPE_CERTIFICATE);
	g_value_set_object (&__params_it->value, certificate);
	__params_it++;
	self = g_object_newv (SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER, __params_it - __params, __params);
	while (__params_it > __params) {
		--__params_it;
		g_value_unset (&__params_it->value);
	}
	g_free (__params);
	return self;
}


static void seahorse_pkcs11_source_remover_on_destroy_object (SeahorsePkcs11SourceRemover* self, GObject* obj, GAsyncResult* result) {
	GError * inner_error;
	GP11Object* _tmp0;
	GP11Object* object;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_REMOVER (self));
	g_return_if_fail (G_IS_OBJECT (obj));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	inner_error = NULL;
	_tmp0 = NULL;
	object = (_tmp0 = GP11_OBJECT (obj), (_tmp0 == NULL ? NULL : g_object_ref (_tmp0)));
	{
		gp11_object_destroy_finish (object, result, &inner_error);
		if (inner_error != NULL) {
			goto __catch6_g_error;
		}
	}
	goto __finally6;
	__catch6_g_error:
	{
		GError * err;
		err = inner_error;
		inner_error = NULL;
		{
			if (err->code == CKR_FUNCTION_CANCELED) {
				seahorse_operation_mark_done (SEAHORSE_OPERATION (self), TRUE, NULL);
			} else {
				GError* _tmp1;
				_tmp1 = NULL;
				seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, (_tmp1 = err, (_tmp1 == NULL || g_error_copy == NULL ? ((gpointer) (_tmp1)) : g_error_copy (((gpointer) (_tmp1))))));
			}
			(err == NULL ? NULL : (err = (g_error_free (err), NULL)));
			(object == NULL ? NULL : (object = (g_object_unref (object), NULL)));
			return;
		}
	}
	__finally6:
	;
	seahorse_context_remove_object (seahorse_context_for_app (), SEAHORSE_OBJECT (seahorse_pkcs11_source_remover_get_certificate (self)));
	seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, NULL);
	(object == NULL ? NULL : (object = (g_object_unref (object), NULL)));
}


/* Cancel the operation */
static void seahorse_pkcs11_source_remover_real_cancel (SeahorseOperation* base) {
	SeahorsePkcs11SourceRemover * self;
	self = SEAHORSE_PKCS11_SOURCE_REMOVER (base);
	g_cancellable_cancel (self->priv->_cancellable);
}


static GCancellable* seahorse_pkcs11_source_remover_get_cancellable (SeahorsePkcs11SourceRemover* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_SOURCE_IS_REMOVER (self), NULL);
	return self->priv->_cancellable;
}


static SeahorsePkcs11Source* seahorse_pkcs11_source_remover_get_source (SeahorsePkcs11SourceRemover* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_SOURCE_IS_REMOVER (self), NULL);
	return self->priv->_source;
}


static void seahorse_pkcs11_source_remover_set_source (SeahorsePkcs11SourceRemover* self, SeahorsePkcs11Source* value) {
	SeahorsePkcs11Source* _tmp2;
	SeahorsePkcs11Source* _tmp1;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_REMOVER (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->priv->_source = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))), (self->priv->_source == NULL ? NULL : (self->priv->_source = (g_object_unref (self->priv->_source), NULL))), _tmp2);
	g_object_notify (((GObject *) (self)), "source");
}


static SeahorsePkcs11Certificate* seahorse_pkcs11_source_remover_get_certificate (SeahorsePkcs11SourceRemover* self) {
	g_return_val_if_fail (SEAHORSE_PKCS11_SOURCE_IS_REMOVER (self), NULL);
	return self->priv->_certificate;
}


static void seahorse_pkcs11_source_remover_set_certificate (SeahorsePkcs11SourceRemover* self, SeahorsePkcs11Certificate* value) {
	SeahorsePkcs11Certificate* _tmp2;
	SeahorsePkcs11Certificate* _tmp1;
	g_return_if_fail (SEAHORSE_PKCS11_SOURCE_IS_REMOVER (self));
	_tmp2 = NULL;
	_tmp1 = NULL;
	self->priv->_certificate = (_tmp2 = (_tmp1 = value, (_tmp1 == NULL ? NULL : g_object_ref (_tmp1))), (self->priv->_certificate == NULL ? NULL : (self->priv->_certificate = (g_object_unref (self->priv->_certificate), NULL))), _tmp2);
	g_object_notify (((GObject *) (self)), "certificate");
}


static void _seahorse_pkcs11_source_remover_on_destroy_object_gasync_ready_callback (GObject* source_object, GAsyncResult* res, gpointer self) {
	seahorse_pkcs11_source_remover_on_destroy_object (self, source_object, res);
}


static GObject * seahorse_pkcs11_source_remover_constructor (GType type, guint n_construct_properties, GObjectConstructParam * construct_properties) {
	GObject * obj;
	SeahorsePkcs11SourceRemoverClass * klass;
	GObjectClass * parent_class;
	SeahorsePkcs11SourceRemover * self;
	klass = SEAHORSE_PKCS11_SOURCE_REMOVER_CLASS (g_type_class_peek (SEAHORSE_PKCS11_SOURCE_TYPE_REMOVER));
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
	obj = parent_class->constructor (type, n_construct_properties, construct_properties);
	self = SEAHORSE_PKCS11_SOURCE_REMOVER (obj);
	{
		GCancellable* _tmp5;
		_tmp5 = NULL;
		self->priv->_cancellable = (_tmp5 = g_cancellable_new (), (self->priv->_cancellable == NULL ? NULL : (self->priv->_cancellable = (g_object_unref (self->priv->_cancellable), NULL))), _tmp5);
		gp11_object_destroy_async (seahorse_pkcs11_certificate_get_pkcs11_object (self->priv->_certificate), self->priv->_cancellable, _seahorse_pkcs11_source_remover_on_destroy_object_gasync_ready_callback, self);
		seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	}
	return obj;
}


static void seahorse_pkcs11_source_remover_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11SourceRemover * self;
	self = SEAHORSE_PKCS11_SOURCE_REMOVER (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_REMOVER_CANCELLABLE:
		g_value_set_object (value, seahorse_pkcs11_source_remover_get_cancellable (self));
		break;
		case SEAHORSE_PKCS11_SOURCE_REMOVER_SOURCE:
		g_value_set_object (value, seahorse_pkcs11_source_remover_get_source (self));
		break;
		case SEAHORSE_PKCS11_SOURCE_REMOVER_CERTIFICATE:
		g_value_set_object (value, seahorse_pkcs11_source_remover_get_certificate (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_remover_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11SourceRemover * self;
	self = SEAHORSE_PKCS11_SOURCE_REMOVER (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_REMOVER_SOURCE:
		seahorse_pkcs11_source_remover_set_source (self, g_value_get_object (value));
		break;
		case SEAHORSE_PKCS11_SOURCE_REMOVER_CERTIFICATE:
		seahorse_pkcs11_source_remover_set_certificate (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_remover_class_init (SeahorsePkcs11SourceRemoverClass * klass) {
	seahorse_pkcs11_source_remover_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11SourceRemoverPrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pkcs11_source_remover_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_pkcs11_source_remover_set_property;
	G_OBJECT_CLASS (klass)->constructor = seahorse_pkcs11_source_remover_constructor;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pkcs11_source_remover_dispose;
	SEAHORSE_OPERATION_CLASS (klass)->cancel = seahorse_pkcs11_source_remover_real_cancel;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_REMOVER_CANCELLABLE, g_param_spec_object ("cancellable", "cancellable", "cancellable", G_TYPE_CANCELLABLE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_REMOVER_SOURCE, g_param_spec_object ("source", "source", "source", SEAHORSE_PKCS11_TYPE_SOURCE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_REMOVER_CERTIFICATE, g_param_spec_object ("certificate", "certificate", "certificate", SEAHORSE_PKCS11_TYPE_CERTIFICATE, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}


static void seahorse_pkcs11_source_remover_instance_init (SeahorsePkcs11SourceRemover * self) {
	self->priv = SEAHORSE_PKCS11_SOURCE_REMOVER_GET_PRIVATE (self);
}


static void seahorse_pkcs11_source_remover_dispose (GObject * obj) {
	SeahorsePkcs11SourceRemover * self;
	self = SEAHORSE_PKCS11_SOURCE_REMOVER (obj);
	(self->priv->_cancellable == NULL ? NULL : (self->priv->_cancellable = (g_object_unref (self->priv->_cancellable), NULL)));
	(self->priv->_source == NULL ? NULL : (self->priv->_source = (g_object_unref (self->priv->_source), NULL)));
	(self->priv->_certificate == NULL ? NULL : (self->priv->_certificate = (g_object_unref (self->priv->_certificate), NULL)));
	G_OBJECT_CLASS (seahorse_pkcs11_source_remover_parent_class)->dispose (obj);
}


static GType seahorse_pkcs11_source_remover_get_type (void) {
	static GType seahorse_pkcs11_source_remover_type_id = 0;
	if (G_UNLIKELY (seahorse_pkcs11_source_remover_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePkcs11SourceRemoverClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pkcs11_source_remover_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePkcs11SourceRemover), 0, (GInstanceInitFunc) seahorse_pkcs11_source_remover_instance_init };
		seahorse_pkcs11_source_remover_type_id = g_type_register_static (SEAHORSE_TYPE_OPERATION, "SeahorsePkcs11SourceRemover", &g_define_type_info, 0);
	}
	return seahorse_pkcs11_source_remover_type_id;
}


static void seahorse_pkcs11_source_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11Source * self;
	self = SEAHORSE_PKCS11_SOURCE (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_SLOT:
		g_value_set_object (value, seahorse_pkcs11_source_get_slot (self));
		break;
		case SEAHORSE_PKCS11_SOURCE_LOCATION:
		g_value_set_enum (value, seahorse_pkcs11_source_get_location (self));
		break;
		case SEAHORSE_PKCS11_SOURCE_KEY_TYPE:
		g_value_set_uint (value, seahorse_pkcs11_source_get_key_type (self));
		break;
		case SEAHORSE_PKCS11_SOURCE_KEY_DESC:
		g_value_set_string (value, seahorse_pkcs11_source_get_key_desc (self));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec) {
	SeahorsePkcs11Source * self;
	self = SEAHORSE_PKCS11_SOURCE (object);
	switch (property_id) {
		case SEAHORSE_PKCS11_SOURCE_SLOT:
		seahorse_pkcs11_source_set_slot (self, g_value_get_object (value));
		break;
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void seahorse_pkcs11_source_class_init (SeahorsePkcs11SourceClass * klass) {
	seahorse_pkcs11_source_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorsePkcs11SourcePrivate));
	G_OBJECT_CLASS (klass)->get_property = seahorse_pkcs11_source_get_property;
	G_OBJECT_CLASS (klass)->set_property = seahorse_pkcs11_source_set_property;
	G_OBJECT_CLASS (klass)->dispose = seahorse_pkcs11_source_dispose;
	SEAHORSE_SOURCE_CLASS (klass)->load = seahorse_pkcs11_source_real_load;
	SEAHORSE_PKCS11_SOURCE_CLASS (klass)->import = seahorse_pkcs11_source_real_import;
	SEAHORSE_PKCS11_SOURCE_CLASS (klass)->export = seahorse_pkcs11_source_real_export;
	SEAHORSE_PKCS11_SOURCE_CLASS (klass)->remove = seahorse_pkcs11_source_real_remove;
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_SLOT, g_param_spec_object ("slot", "slot", "slot", GP11_TYPE_SLOT, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_LOCATION, g_param_spec_enum ("location", "location", "location", SEAHORSE_TYPE_LOCATION, 0, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_KEY_TYPE, g_param_spec_uint ("key-type", "key-type", "key-type", 0, G_MAXUINT, 0U, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (klass), SEAHORSE_PKCS11_SOURCE_KEY_DESC, g_param_spec_string ("key-desc", "key-desc", "key-desc", NULL, G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_READABLE));
}


static void seahorse_pkcs11_source_instance_init (SeahorsePkcs11Source * self) {
	self->priv = SEAHORSE_PKCS11_SOURCE_GET_PRIVATE (self);
}


static void seahorse_pkcs11_source_dispose (GObject * obj) {
	SeahorsePkcs11Source * self;
	self = SEAHORSE_PKCS11_SOURCE (obj);
	(self->priv->_slot == NULL ? NULL : (self->priv->_slot = (g_object_unref (self->priv->_slot), NULL)));
	G_OBJECT_CLASS (seahorse_pkcs11_source_parent_class)->dispose (obj);
}


GType seahorse_pkcs11_source_get_type (void) {
	static GType seahorse_pkcs11_source_type_id = 0;
	if (G_UNLIKELY (seahorse_pkcs11_source_type_id == 0)) {
		static const GTypeInfo g_define_type_info = { sizeof (SeahorsePkcs11SourceClass), (GBaseInitFunc) NULL, (GBaseFinalizeFunc) NULL, (GClassInitFunc) seahorse_pkcs11_source_class_init, (GClassFinalizeFunc) NULL, NULL, sizeof (SeahorsePkcs11Source), 0, (GInstanceInitFunc) seahorse_pkcs11_source_instance_init };
		seahorse_pkcs11_source_type_id = g_type_register_static (SEAHORSE_TYPE_SOURCE, "SeahorsePkcs11Source", &g_define_type_info, 0);
	}
	return seahorse_pkcs11_source_type_id;
}




