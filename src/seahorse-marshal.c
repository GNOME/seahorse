/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#include "seahorse-marshal.h"

void
seahorse_marshal_VOID__STRING_DOUBLE (GClosure *closure, GValue *return_value,
				      guint n_param_values, const GValue *param_values,
				      gpointer invocation_hint, gpointer marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_DOUBLE) (gpointer data1,
		gchar *arg1, gdouble arg2, gpointer data2);
	
	register GMarshalFunc_VOID__STRING_DOUBLE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail (n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	
	callback = (GMarshalFunc_VOID__STRING_DOUBLE) (marshal_data ? marshal_data : cc->callback);

	callback (data1, (gchar*)g_value_get_string (param_values + 1),
		g_value_get_double (param_values + 2), data2);
}
