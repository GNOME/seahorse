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

#include "seahorse-gpgmex.h"
#include "seahorse-context.h"
#include "seahorse-windows.h"
#include "seahorse-op.h"
#include "seahorse-util.h"
#include "seahorse-libdialogs.h"

typedef enum _CmdLineMode {
    MODE_NONE,
    MODE_IMPORT,
    MODE_ENCRYPT,
    MODE_SIGN,
    MODE_ENCRYPT_SIGN,
    MODE_DECRYPT,
    MODE_VERIFY
} CmdLineMode;

static gboolean read_uris = FALSE;
static CmdLineMode mode = MODE_NONE; 

static const struct poptOption options[] = {
	{ "import", 'i', POPT_ARG_NONE | POPT_ARG_VAL, &mode, MODE_IMPORT,
	  N_("Import keys from the file"), NULL },

	{ "encrypt", 'e', POPT_ARG_NONE | POPT_ARG_VAL, &mode, MODE_ENCRYPT,
	  N_("Encrypt file"), NULL },

	{ "sign", 's', POPT_ARG_NONE | POPT_ARG_VAL, &mode, MODE_SIGN,
	  N_("Sign file with default key"), NULL },
	
	{ "encrypt-sign", 'n', POPT_ARG_NONE | POPT_ARG_VAL, &mode, MODE_ENCRYPT_SIGN,
	  N_("Encrypt and sign file with default key"), NULL },
	
	{ "decrypt", 'd', POPT_ARG_NONE | POPT_ARG_VAL, &mode, MODE_DECRYPT,
	  N_("Decrypt encrypted file"), NULL },
	
	{ "verify", 'v', POPT_ARG_NONE | POPT_ARG_VAL, &mode, MODE_VERIFY,
	  N_("Verify signature file"), NULL },
      
    { "uri-list", 'T', POPT_ARG_NONE | POPT_ARG_VAL, &read_uris, TRUE,
      N_("Read list of URIs on standard in"), NULL },
	
	{ NULL, '\0', 0, NULL, 0, NULL, NULL }
};


/* Returns a null terminated array of uris, use g_strfreev to free */
static gchar** 
read_uri_arguments (GnomeProgram *program)
{
    /* Read uris from stdin */
    if (read_uris) {
       
        GIOChannel* io;
        GArray* files;
        gchar* line;

        files = g_array_new (TRUE, TRUE, sizeof (gchar*));
    
        /* Opens a channel on stdin */
        io = g_io_channel_unix_new (0);
    
        while (g_io_channel_read_line (io, &line, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
     
            if (line == NULL)
                continue;

            g_strstrip(line);
            if(line[0] == 0)
            {
                g_free(line);
                continue;
            }
            
            g_array_append_val(files, line);
        }
        
        g_io_channel_unref (io);
        return (gchar**)g_array_free (files, FALSE);
        
    /* Multiple arguments on command line */
    } else {
       
        poptContext ctx;
        GValue value = { 0, };

        g_value_init (&value, G_TYPE_POINTER);
        g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
        
        ctx = g_value_get_pointer (&value);
        g_value_unset (&value);

        return seahorse_util_strvec_dup (poptGetArgs(ctx));
    }
}

/* Perform an import on the given set of paths */
static guint
do_import (SeahorseContext *sctx, const gchar **paths)
{
    SeahorseKeySource *sksrc;
    GtkWidget *dlg;
    GError *err = NULL;
    gint keys = 0;
    gchar **uris;
    gchar **u;
    gchar *t;
    guint ret = 0;
    
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 1);
    
    /* Get all sub-folders etc... */
    uris = seahorse_util_uris_expand (paths);
    g_assert (uris != NULL);
     
    for (u = uris; *u; u++) {
        keys += seahorse_op_import_file (sksrc, *u, &err);
       
        if (err != NULL) {
            seahorse_util_handle_error (err, _("Couldn't import keys from \"%s\""),
                    seahorse_util_uri_get_last (*u));
            ret = 1;
            break;
        }
    }

    if (ret == 0) {
        t = g_strdup_printf (keys == 1 ? _("Imported key") : 
                                    _("Imported %d keys"), keys);
        dlg = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                             GTK_MESSAGE_INFO, GTK_BUTTONS_OK, t);
        gtk_dialog_run (GTK_DIALOG (dlg));
        gtk_widget_destroy (dlg);
    }

    g_strfreev(uris);
    return ret;
}

/* Encrypt or sign the given set of paths */
static guint 
do_encrypt_base (SeahorseContext *sctx, const gchar **paths, SeahorseKeyPair *signer)
{
    SeahorseKeySource *sksrc;
    GList *keys = NULL;
    gchar *new_path;
    gpgme_error_t err;
    gchar **uris = NULL;
    gchar **u;
    guint ret = 0;

    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 1);
    
    /* This can change the list of uris and optionally compress etc... */
    uris = seahorse_process_multiple (sctx, paths, NULL);

    if(!uris)
        ret = 1;
    else {
        keys = seahorse_recipients_get (sctx);
           
        if (g_list_length (keys) > 0) {
            for(u = uris; *u; u++) {
               
                new_path = seahorse_util_add_suffix (sksrc->ctx, *u, SEAHORSE_CRYPT_SUFFIX, 
                                                     _("Choose Encrypted File Name for '%s'"));
                if (!new_path) 
                    break;
                    
                if (signer)
                    seahorse_op_encrypt_sign_file (keys, signer, *u, new_path, &err);
                else
                    seahorse_op_encrypt_file (keys, *u, new_path, &err);
                
                g_free (new_path);
    
                if (!GPG_IS_OK (err)) {
                    seahorse_util_handle_gpgme (err, _("Couldn't encrypt \"%s\""), 
                        seahorse_util_uri_get_last (*u));
                    ret = 1;
                    break;
                }
            }
            
            g_list_free (keys);
       }

        g_strfreev (uris);
    }
    
    return ret;     
}

/* Encrypt the given set of paths */
static guint
do_encrypt (SeahorseContext *sctx, const gchar **paths)
{
    return do_encrypt_base (sctx, paths, NULL);
}

/* Encrypt and sign the given set of paths */
static guint
do_encrypt_sign (SeahorseContext *sctx, const gchar **paths)
{
    return do_encrypt_base (sctx, paths, seahorse_context_get_default_key (sctx));
}

static guint
do_sign (SeahorseContext *sctx, const gchar **paths)
{
    SeahorseKeySource *sksrc;
    SeahorseKeyPair *signer;
    gpgme_error_t err;
    gchar **uris;
    gchar **u;
    gchar *new_path;
    guint ret = 0;
    
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 1);
    
    signer = seahorse_context_get_default_key (sctx);
    g_return_val_if_fail (signer != NULL, 1);
    
    uris = seahorse_util_uris_expand (paths);
    g_assert (uris != NULL);
     
    for (u = uris; *u; u++) {
      
        new_path = seahorse_util_add_suffix (sksrc->ctx, *u, SEAHORSE_SIG_SUFFIX, 
                                             _("Choose Signature File Name for '%s'"));
        if (!new_path)
            break;
        
        seahorse_op_sign_file (signer, *u, new_path, &err);
        g_free(new_path);
        
        if (!GPG_IS_OK (err)) {
            seahorse_util_handle_gpgme (err, _("Couldn't sign \"%s\""),
                seahorse_util_uri_get_last (*u));
            ret = 1;
            break;
        }
    }

    g_strfreev(uris);    
    return ret;
}

/* Decrypt the given set of paths */
static guint
do_decrypt (SeahorseContext *sctx, const gchar **paths)
{
    SeahorseKeySource *sksrc;
    gpgme_verify_result_t status = NULL;
    SeahorseWidget *signatures = NULL;
    gpgme_error_t err;
    gchar **uris;
    gchar **u;
    gchar *new_path;
    guint ret = 0;
	
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 1);
        
    uris = seahorse_util_uris_expand (paths);
    g_assert (uris != NULL);
     
    for (u = uris; *u; u++) {
      
        new_path = seahorse_util_remove_suffix (*u, _("Choose Decrypted File Name for '%s'"));
        if(!new_path)
            break;
            
        seahorse_op_decrypt_verify_file (sksrc, *u, new_path, &status, &err);
                
        if (!GPG_IS_OK (err)) {
            seahorse_util_handle_gpgme (err, _("Couldn't decrypt \"%s\""),
                    seahorse_util_uri_get_last (*u));
            ret = 1;
            break;
        }
    
        if(status && status->signatures) {
            if(!signatures)
                signatures = seahorse_signatures_new (sctx);
            seahorse_signatures_add (sctx, signatures, new_path, status);
        }

        g_free (new_path);
    }
    
    g_strfreev (uris);    
    
    if (ret == 0 && signatures)
        seahorse_signatures_run (sctx, signatures);
        
    return ret;
}

/* Verify the given set of paths. Prompt user if original files not found */
static guint
do_verify (SeahorseContext *sctx, const gchar **paths)
{
    SeahorseKeySource *sksrc;
    gpgme_verify_result_t status = NULL;
    SeahorseWidget *signatures = NULL;
    gchar *original = NULL;
    gpgme_error_t err;
    gchar **uris;
    gchar **u;
    guint ret = 0;
    
    uris = seahorse_util_uris_expand (paths);
    g_assert (uris != NULL);
	
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 1);
         
    for (u = uris; *u; u++) {

        original = seahorse_util_remove_suffix (*u, NULL);

        /* The original file doesn't exist, prompt for it */
        if (!seahorse_util_uri_exists (original)) {
           
            GtkWidget *dialog;
            gchar *t;
            
            t = g_strdup_printf (_("Choose Original File for '%s'"), 
                    seahorse_util_uri_get_last (*u));
            
            dialog = gtk_file_chooser_dialog_new (t, 
                        NULL, GTK_FILE_CHOOSER_ACTION_OPEN, 
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                        NULL);
            
            g_free (t);

            gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dialog), original);
            gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);

            g_free (original);                
            original = NULL;
            
            if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) 
                original = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
                
            gtk_widget_destroy (dialog);
        }
        
        if (original) {
            seahorse_op_verify_file (sksrc, *u, original, &status, &err);
            g_free (original);

            if (!GPG_IS_OK (err)) {
                seahorse_util_handle_gpgme (err, _("Couldn't verify \"%s\""),
                        seahorse_util_uri_get_last (*u));
                ret = 1;
                break;
            }
            
            if (status && status->signatures) {
                if(!signatures)
                    signatures = seahorse_signatures_new (sctx);
                seahorse_signatures_add (sctx, signatures, *u, status);
            }
        }
    }
    
    g_strfreev (uris);    
    
    if (ret == 0 && signatures)
        seahorse_signatures_run (sctx, signatures);
        
    return ret;
}

/* Initializes context and preferences, then loads key manager */
int
main (int argc, char **argv)
{
    GnomeProgram *program = NULL;
	SeahorseContext *sctx;
    GtkWindow* win;
    int ret = 0;

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

    program = gnome_program_init(PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
                    GNOME_PARAM_POPT_TABLE, options,
                    GNOME_PARAM_HUMAN_READABLE_NAME, _("GPG Keys Manager"),
                    GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);

    sctx = seahorse_context_new ();
    seahorse_context_load_keys (sctx, FALSE);

    if (mode != MODE_NONE) {
        gchar **uris = NULL;
       
        /* Load up all our arguments */
        uris = read_uri_arguments(program);
    
        if(uris && uris[0]) {
            switch (mode) {
            case MODE_IMPORT:
                ret = do_import (sctx, (const gchar**)uris);
                break;
            case MODE_ENCRYPT:
                ret = do_encrypt (sctx, (const gchar**)uris);
                break;
            case MODE_SIGN:
                ret = do_sign (sctx, (const gchar**)uris);
                break;
            case MODE_ENCRYPT_SIGN:
                ret = do_encrypt_sign (sctx, (const gchar**)uris);
                break;
            case MODE_DECRYPT:
                ret = do_decrypt (sctx, (const gchar**)uris);
                break;
            case MODE_VERIFY:
                ret = do_verify (sctx, (const gchar**)uris);
                break;
            default:
                g_assert_not_reached ();
                break;
            };
                
            g_strfreev (uris);
        }
        
        return ret;
    }
 
    win = seahorse_key_manager_show (sctx);
    g_signal_connect_after (G_OBJECT (win), "destroy", gtk_main_quit, NULL);
	gtk_main ();
	
	return 0;
}
