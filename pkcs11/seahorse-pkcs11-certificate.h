
#ifndef __SEAHORSE_PKCS11_CERTIFICATE_H__
#define __SEAHORSE_PKCS11_CERTIFICATE_H__

#include <glib.h>
#include <glib-object.h>
#include <seahorse-object.h>
#include <gp11.h>
#include <gp11-hacks.h>
#include <stdlib.h>
#include <string.h>

G_BEGIN_DECLS


#define SEAHORSE_PKCS11_TYPE_CERTIFICATE (seahorse_pkcs11_certificate_get_type ())
#define SEAHORSE_PKCS11_CERTIFICATE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11Certificate))
#define SEAHORSE_PKCS11_CERTIFICATE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11CertificateClass))
#define SEAHORSE_PKCS11_IS_CERTIFICATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEAHORSE_PKCS11_TYPE_CERTIFICATE))
#define SEAHORSE_PKCS11_IS_CERTIFICATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SEAHORSE_PKCS11_TYPE_CERTIFICATE))
#define SEAHORSE_PKCS11_CERTIFICATE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SEAHORSE_PKCS11_TYPE_CERTIFICATE, SeahorsePkcs11CertificateClass))

typedef struct _SeahorsePkcs11Certificate SeahorsePkcs11Certificate;
typedef struct _SeahorsePkcs11CertificateClass SeahorsePkcs11CertificateClass;
typedef struct _SeahorsePkcs11CertificatePrivate SeahorsePkcs11CertificatePrivate;

struct _SeahorsePkcs11Certificate {
	SeahorseObject parent_instance;
	SeahorsePkcs11CertificatePrivate * priv;
};

struct _SeahorsePkcs11CertificateClass {
	SeahorseObjectClass parent_class;
};


SeahorsePkcs11Certificate* seahorse_pkcs11_certificate_new (GP11Object* object, GP11Attributes* attributes);
GP11Object* seahorse_pkcs11_certificate_get_pkcs11_object (SeahorsePkcs11Certificate* self);
void seahorse_pkcs11_certificate_set_pkcs11_object (SeahorsePkcs11Certificate* self, GP11Object* value);
GP11Attributes* seahorse_pkcs11_certificate_get_pkcs11_attributes (SeahorsePkcs11Certificate* self);
void seahorse_pkcs11_certificate_set_pkcs11_attributes (SeahorsePkcs11Certificate* self, GP11Attributes* value);
char* seahorse_pkcs11_certificate_get_display_id (SeahorsePkcs11Certificate* self);
const char* seahorse_pkcs11_certificate_get_simple_name (SeahorsePkcs11Certificate* self);
char* seahorse_pkcs11_certificate_get_fingerprint (SeahorsePkcs11Certificate* self);
gint seahorse_pkcs11_certificate_get_validity (SeahorsePkcs11Certificate* self);
char* seahorse_pkcs11_certificate_get_validity_str (SeahorsePkcs11Certificate* self);
gint seahorse_pkcs11_certificate_get_trust (SeahorsePkcs11Certificate* self);
char* seahorse_pkcs11_certificate_get_trust_str (SeahorsePkcs11Certificate* self);
gulong seahorse_pkcs11_certificate_get_expires (SeahorsePkcs11Certificate* self);
char* seahorse_pkcs11_certificate_get_expires_str (SeahorsePkcs11Certificate* self);
GType seahorse_pkcs11_certificate_get_type (void);


G_END_DECLS

#endif
