
#include <libbonobo.h>
#include <config.h>

#include "seahorse-context.h"

static BonoboObject*
context_factory (BonoboGenericFactory *factory, const char **iid, gpointer data)
{
	static SeahorseContext *sctx = NULL;
	
	if (g_str_equal (iid, "OAFIID:Seahorse_Context:new"))
		return seahorse_context_new ();
	if (g_str_equal (iid, "OAFIID:Seahorse_Context") && sctx == NULL)
		sctx = seahorse_context_new ();
	else if (sctx != NULL)
		BONOBO_OBJREF (sctx);
	
	return sctx;
}

BONOBO_ACTIVATION_FACTORY ("OAFIID:Seahorse_Context_Factory", "SeahorseContext Factory",
	"1.0", (BonoboFactoryCallback)context_factory, NULL);
