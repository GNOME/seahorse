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

#include <config.h>
#include <gnome.h>

#include "seahorse-context.h"
#include "seahorse-windows.h"

static gchar *import = NULL;
static gchar *encrypt = NULL;
static gchar *sign = NULL;
static gchar *encrypt_sign = NULL;
static gchar *decrypt = NULL;
static gchar *verify = NULL;
static gchar *decrypt_verify = NULL;

static const struct poptOption options[] = {
	{ "import", 'i', POPT_ARG_STRING, &import, 0,
	  N_("Import keys from the file"), N_("FILE") },

	{ "encrypt", 'e', POPT_ARG_STRING, &encrypt, 0,
	  N_("Encrypt file"), N_("FILE") },

	{ "sign", 's', POPT_ARG_STRING, &sign, 0,
	  N_("Sign file with default key"), N_("FILE") },
	
	{ "encrypt-sign", 'n', POPT_ARG_STRING, &encrypt_sign, 0,
	  N_("Encrypt and sign file with default key"), N_("FILE") },
	
	{ "decrypt", 'd', POPT_ARG_STRING, &decrypt, 0,
	  N_("Decrypt encrypted file"), N_("FILE") },
	
	{ "verify", 'v', POPT_ARG_STRING, &verify, 0,
	  N_("Verify signature file"), N_("FILE") },
	
	{ "decrypt-verify", 'r', POPT_ARG_STRING, &decrypt_verify, 0,
	  N_("Decrypt encrypt file and verify any signatures it contains"), N_("FILE") },
	
	{ NULL, '\0', 0, NULL, 0 }
};

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
	SeahorseContext *sctx;

#ifdef ENABLE_NLS	
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
		GNOME_PARAM_POPT_TABLE, options,
		GNOME_PARAM_HUMAN_READABLE_NAME, _("GPG Keys Manager"),
		GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);
	
	sctx = seahorse_context_new ();
	
	if (import != NULL) {
		g_printf ("import file %s\n", import);
		return 0;
	}
	if (encrypt != NULL) {
		g_printf ("encrypt file %s\n", encrypt);
		return 0;
	}
	if (sign != NULL) {
		g_printf ("sign file %s\n", sign);
		return 0;
	}
	if (encrypt_sign != NULL) {
		g_printf ("encrypt & sign file %s\n", encrypt_sign);
		return 0;
	}
	if (decrypt != NULL) {
		g_printf ("decrypt file %s\n", decrypt);
		return 0;
	}
	if (verify != NULL) {
		g_printf ("verify file %s\n", verify);
		return 0;
	}
	if (decrypt_verify != NULL) {
		g_printf ("decrypt & verify file %s\n", decrypt_verify);
		return 0;
	}
	
	seahorse_key_manager_show (sctx);
	gtk_main ();
	
	return 0;
}
