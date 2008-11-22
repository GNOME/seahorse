
#include "config.h"

#include "crui-util.h"

static const char HEXC[] = "0123456789ABCDEF";

gchar*
_crui_util_encode_hex (const guchar *data, gsize n_data)
{
	GString *result = g_string_sized_new (n_data * 2 + 1);
	gsize i;
	
	g_return_val_if_fail (data, NULL);
		
	for (i = 0; i < n_data; ++i) {
		g_string_append_c (result, HEXC[data[i] >> 4 & 0xf]);
		g_string_append_c (result, HEXC[data[i] & 0xf]);
	}
		
	return g_string_free (result, FALSE);
}
