
#include "config.h"
#include "run-tests.h"

#include "crui-util.h"

#include <glib.h>

#include <string.h>

DEFINE_TEST (encode_hex)
{
	gchar *value;
	
	value = _crui_util_encode_hex ((const guchar*)"blahblah", 8);
	g_assert (value != NULL);
	g_assert_cmpstr (value, ==, "626C6168626C6168");
	g_free (value);
}

