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
#include <locale.h>

#include "seahorse-context.h"
#include "seahorse-windows.h"
#include "seahorse-op.h"
#include "seahorse-util.h"
#include "seahorse-libdialogs.h"

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

static void
show_info (const gchar *message)
{
	GtkWidget *widget;
	
	widget = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_INFO, GTK_BUTTONS_OK, message);
	gtk_dialog_run (GTK_DIALOG (widget));
	gtk_widget_destroy (widget);
}

static void
do_encrypt (SeahorseContext *sctx, gchar *path,
	    SeahorseEncryptFunc func, const gchar *message)
{
	gpgme_key_t * recips = NULL;
	gchar *new_path;
	gpgme_error_t err;
		
	recips = seahorse_recipients_get (sctx);
		
	if (recips == NULL) {
		g_free (path);
		exit (1);
	}
		
	new_path = func (sctx, path, recips, &err);
	g_free (path);
		
	if (!GPG_IS_OK (err)) {
		seahorse_util_handle_error (err);
		exit (1);
	}
	else {
		show_info (g_strdup_printf (message, new_path));
		g_free (new_path);
		exit (0);
	}
}

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
	SeahorseContext *sctx;
	gpgme_error_t err;
	gchar *new_path;
    GtkWindow* win = NULL;

#ifdef ENABLE_NLS	
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	g_message("init gpgme version %s", gpgme_check_version(NULL));

#ifdef ENABLE_NLS	
	gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
	gpgme_set_locale(NULL, LC_MESSAGES, setlocale(LC_MESSAGES, NULL));
#endif

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
		GNOME_PARAM_POPT_TABLE, options,
		GNOME_PARAM_HUMAN_READABLE_NAME, _("GPG Keys Manager"),
		GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);
	
	sctx = seahorse_context_new ();
	
	if (import != NULL) {
		gint keys;
		
		keys = seahorse_op_import_file (sctx, import, &err);
		
		if (!GPG_IS_OK (err)) {
			seahorse_util_handle_error (err);
			return 1;
		}
		else {
			show_info (g_strdup_printf (keys == 1 ? _("Imported key") : 
                                            _("Imported %d keys"), keys));
			return 0;
		}
	}
 
	else if (encrypt != NULL) {
		do_encrypt (sctx, encrypt, seahorse_op_encrypt_file, _("Encrypt file is %s"));
    }
    
	else if (sign != NULL) {
		new_path = seahorse_op_sign_file (sctx, sign, &err);
		
		if (!GPG_IS_OK (err)) {
			seahorse_util_handle_error (err);
			return 1;
		}
		else {
			show_info (g_strdup_printf (_("Signature file is %s"), new_path));
			g_free (new_path);
			return 0;
		}
	}
 
	else if (encrypt_sign != NULL) {
    	do_encrypt (sctx, encrypt_sign, seahorse_op_encrypt_sign_file,
			_("Encrypted and signed file is %s"));
    }
          
	else if (decrypt != NULL) {
		new_path = seahorse_op_decrypt_file (sctx, decrypt, &err);
		
		if (!GPG_IS_OK (err)) {
			seahorse_util_handle_error (err);
			return 1;
		}
		else {
			show_info (g_strdup_printf (_("Decrypted file is %s"), new_path));
			g_free (new_path);
			return 0;
		}
	}
 
	else if (verify != NULL) {
		gpgme_verify_result_t status;
		
		seahorse_op_verify_file (sctx, verify, &status, &err);
		
		if (!GPG_IS_OK (err)) {
			seahorse_util_handle_error (err);
			return 1;
		}
		else {
			win = seahorse_signatures_new (sctx, status);
		}
	}
 
    else if (decrypt_verify != NULL) {
		gpgme_verify_result_t status;
		
		new_path = seahorse_op_decrypt_verify_file (sctx,
			decrypt_verify, &status, &err);
		
		if (!GPG_IS_OK (err)) {
			seahorse_util_handle_error (err);
			return 1;
		}
		else {
			show_info (g_strdup_printf (_("Decrypted file is %s"), new_path));
			g_free (new_path);
			
			win = seahorse_signatures_new (sctx, status);
		}
	}
    
    if(!win)
        win = seahorse_key_manager_show (sctx);
    
    g_signal_connect_after(G_OBJECT (win), "destroy", gtk_main_quit, NULL);
	gtk_main ();
	
	return 0;
}
