
#ifndef __SEAHORSE_PKCS11_SOURCE_H__
#define __SEAHORSE_PKCS11_SOURCE_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-source.h>
#include <gp11.h>
#include <seahorse-operation.h>
#include <gio/gio.h>
#include <seahorse-object.h>
#include <seahorse-types.h>
#include <stdlib.h>
#include <string.h>

G_BEGIN_DECLS


#define SEAHORSE_PKCS11_TYPE_SOURCE (seahorse_pkcs11_source_get_type ())
#define SEAHORSE_PKCS11_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_TYPE_SOURCE, SeahorsePkcs11Source))
#define SEAHORSE_PKCS11_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_TYPE_SOURCE, SeahorsePkcs11SourceClass))
#define SEAHORSE_PKCS11_IS_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_TYPE_SOURCE))
#define SEAHORSE_PKCS11_IS_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_TYPE_SOURCE))
#define SEAHORSE_PKCS11_SOURCE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_TYPE_SOURCE, SeahorsePkcs11SourceClass))

typedef struct _SeahorsePkcs11Source SeahorsePkcs11Source;
typedef struct _SeahorsePkcs11SourceClass SeahorsePkcs11SourceClass;
typedef struct _SeahorsePkcs11SourcePrivate SeahorsePkcs11SourcePrivate;

struct _SeahorsePkcs11Source {
	SeahorseSource parent_instance;
	SeahorsePkcs11SourcePrivate * priv;
};

struct _SeahorsePkcs11SourceClass {
	SeahorseSourceClass parent_class;
	SeahorseOperation* (*import) (SeahorsePkcs11Source* self, GInputStream* input);
	SeahorseOperation* (*export) (SeahorsePkcs11Source* self, GList* objects, GOutputStream* output);
	SeahorseOperation* (*remove) (SeahorsePkcs11Source* self, SeahorseObject* object);
};


SeahorsePkcs11Source* seahorse_pkcs11_source_new (GP11Slot* slot);
SeahorseOperation* seahorse_pkcs11_source_import (SeahorsePkcs11Source* self, GInputStream* input);
SeahorseOperation* seahorse_pkcs11_source_export (SeahorsePkcs11Source* self, GList* objects, GOutputStream* output);
SeahorseOperation* seahorse_pkcs11_source_remove (SeahorsePkcs11Source* self, SeahorseObject* object);
GP11Slot* seahorse_pkcs11_source_get_slot (SeahorsePkcs11Source* self);
SeahorseLocation seahorse_pkcs11_source_get_location (SeahorsePkcs11Source* self);
GQuark seahorse_pkcs11_source_get_key_type (SeahorsePkcs11Source* self);
const char* seahorse_pkcs11_source_get_key_desc (SeahorsePkcs11Source* self);
GType seahorse_pkcs11_source_get_type (void);


G_END_DECLS

#endif
