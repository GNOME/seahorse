
#include <config.h>
#include <string.h>
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>

#include "seahorse-component.h"

#define VIEW_IID_ALL "OAFIID:Seahorse_Component_All"
#define VIEW_IID_ENCRYPTED "OAFIID:Seahorse_Component_Encrypted"
#define VIEW_IID_SIGNATURE "OAFIID:Seahorse_Component_Signature"
#define VIEW_IID_KEYS "OAFIID:Seahorse_Component_Keys"

static CORBA_Object
image_shlib_make_object (PortableServer_POA poa, const gchar *iid,
			 gpointer impl_ptr, CORBA_Environment *ev)
{
	SeahorseComponent *sc;

	sc = g_object_new (SEAHORSE_TYPE_COMPONENT, NULL);
	bonobo_activation_plugin_use (poa, impl_ptr);
	return CORBA_Object_duplicate (BONOBO_OBJREF (sc), ev);
}

static const BonoboActivationPluginObject plugin_list[] = {
	{ VIEW_IID_ALL, image_shlib_make_object },
	{ VIEW_IID_ENCRYPTED, image_shlib_make_object },
	{ VIEW_IID_SIGNATURE, image_shlib_make_object },
	{ VIEW_IID_KEYS, image_shlib_make_object },
	{ NULL }
};

const BonoboActivationPlugin Bonobo_Plugin_info = {
	plugin_list, "Seahorse Component"
};
