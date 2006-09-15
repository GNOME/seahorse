/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
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

#include "config.h"
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>

#include "cryptui.h"

#include "seahorse-secure-memory.h"
#include "seahorse-tool.h"
#include "seahorse-util.h"
#include "seahorse-vfs-data.h"
#include "seahorse-libdialogs.h"
#include "seahorse-gtkstock.h"
#include "seahorse-gconf.h"
#include "seahorse-util.h"

/* -----------------------------------------------------------------------------
 * ARGUMENT PARSING 
 */

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
static CmdLineMode cmd_mode = MODE_NONE; 

static const struct poptOption options[] = {
    { "import", 'i', POPT_ARG_NONE | POPT_ARG_VAL, &cmd_mode, MODE_IMPORT,
      N_("Import keys from the file"), NULL },
    { "encrypt", 'e', POPT_ARG_NONE | POPT_ARG_VAL, &cmd_mode, MODE_ENCRYPT,
      N_("Encrypt file"), NULL },
    { "sign", 's', POPT_ARG_NONE | POPT_ARG_VAL, &cmd_mode, MODE_SIGN,
      N_("Sign file with default key"), NULL },
    { "encrypt-sign", 'n', POPT_ARG_NONE | POPT_ARG_VAL, &cmd_mode, MODE_ENCRYPT_SIGN,
      N_("Encrypt and sign file with default key"), NULL },
    { "decrypt", 'd', POPT_ARG_NONE | POPT_ARG_VAL, &cmd_mode, MODE_DECRYPT,
      N_("Decrypt encrypted file"), NULL },
    { "verify", 'v', POPT_ARG_NONE | POPT_ARG_VAL, &cmd_mode, MODE_VERIFY,
      N_("Verify signature file"), NULL },
    { "uri-list", 'T', POPT_ARG_NONE | POPT_ARG_VAL, &read_uris, TRUE,
      N_("Read list of URIs on standard in"), NULL },
    
    POPT_AUTOHELP
    POPT_TABLEEND
};

/* Returns a null terminated array of uris, use g_strfreev to free */
static gchar** 
read_uri_arguments (poptContext pctx)
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
            if(line[0] == 0) {
                g_free(line);
                continue;
            }
            g_array_append_val(files, line);
        }
        
        g_io_channel_unref (io);
        return (gchar**)g_array_free (files, FALSE);
        
    /* Multiple arguments on command line */
    } else {
       
        return seahorse_util_strvec_dup (poptGetArgs(pctx));
    }
}

/* -----------------------------------------------------------------------------
 * ENCRYPT SIGN 
 */

static gpgme_key_t*
prompt_recipients (gpgme_key_t *signkey)
{
    gpgme_error_t gerr = GPG_OK;
    CryptUIKeyset *keyset;
    gpgme_ctx_t ctx;
    gpgme_key_t key;
    GArray *keys;
    gchar **recips;
    gchar *signer;
    
    *signkey = NULL;
    
    keyset = cryptui_keyset_new ("openpgp");
    recips = cryptui_prompt_recipients (keyset, _("Choose Recipients"), &signer);
    
    if (recips) {
        
        gerr = gpgme_new (&ctx);
        g_return_val_if_fail (GPG_IS_OK (gerr), NULL);
    
        if (signer) {
            /* Load up the GPGME secret key */
            gchar *id = cryptui_keyset_key_raw_keyid (keyset, signer);
            gerr = gpgme_get_key (ctx, id, signkey, 1);
            g_free (id);
            
            /* A more descriptive error message */
            if (GPG_ERR_EOF == gpgme_err_code (gerr))
                gerr = GPG_E (GPG_ERR_NOT_FOUND);
        }
        
        if (GPG_IS_OK (gerr)) {
            gchar **ids;
            guint num;
            
            /* Load up the GPGME keys */
            ids = cryptui_keyset_keys_raw_keyids (keyset, (const gchar**)recips);
            num = seahorse_util_strvec_length ((const gchar**)ids);
            keys = g_array_new (TRUE, TRUE, sizeof (gpgme_key_t));
            gerr = gpgme_op_keylist_ext_start (ctx, (const gchar**)ids, 0, 0);
            g_free (ids);
            
            if (GPG_IS_OK (gerr)) {
                while (GPG_IS_OK (gerr = gpgme_op_keylist_next (ctx, &key))) 
                    g_array_append_val (keys, key);
                gpgme_op_keylist_end (ctx);
            }
            
            /* Ignore EOF error */
            if (GPG_ERR_EOF == gpgme_err_code (gerr))
                gerr = GPG_OK;
            
            if (GPG_IS_OK (gerr) && num != keys->len)
                g_warning ("couldn't load all the keys (%d/%d) from GPGME", keys->len, num);
        }
        
        gpgme_release (ctx);
    }
    
    g_object_unref (keyset);

    if (!recips) 
        return NULL;

    g_strfreev (recips);
    g_free (signer);
    
    if (GPG_IS_OK (gerr) && keys->len)
        return (gpgme_key_t*)g_array_free (keys, FALSE);
    
    /* When failure, free all our return values */
    seahorse_util_free_keys ((gpgme_key_t*)g_array_free (keys, FALSE));
    if (*signkey)
        gpgmex_key_unref (*signkey);
    
    seahorse_util_handle_gpgme (gerr, _("Couldn't load keys"));
    return NULL;
}

static gboolean
encrypt_sign_start (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata, 
                    SeahorsePGPOperation *pop, GError **err)
{
    gpgme_data_t cipher;
    gpgme_error_t gerr;
    gchar *touri;
    
    g_assert (mode->recipients && mode->recipients[0]);
    
    /* File to encrypt to */
    touri = seahorse_util_add_suffix (uri, SEAHORSE_CRYPT_SUFFIX, 
                                      _("Choose Encrypted File Name for '%s'"));
    if (!touri) 
        return FALSE;

    /* Open necessary files, release these with the operation */
    cipher = seahorse_vfs_data_create (touri, SEAHORSE_VFS_WRITE | SEAHORSE_VFS_DELAY, err);
    g_free (touri);
    if (!cipher) 
        return FALSE;
    g_object_set_data_full (G_OBJECT (pop), "cipher-data", cipher, 
                            (GDestroyNotify)gpgme_data_release);
    
    gpgme_set_armor (pop->gctx, seahorse_gconf_get_boolean (ARMOR_KEY));
    gpgme_set_textmode (pop->gctx, FALSE);

    /* Start actual encryption */
    gpgme_signers_clear (pop->gctx);
    if (mode->signer) {
        gpgme_signers_add (pop->gctx, mode->signer);
        gerr = gpgme_op_encrypt_sign_start (pop->gctx, mode->recipients, 
                                            GPGME_ENCRYPT_ALWAYS_TRUST, uridata, cipher);
        
    } else {
        gerr = gpgme_op_encrypt_start (pop->gctx, mode->recipients, 
                                       GPGME_ENCRYPT_ALWAYS_TRUST, uridata, cipher);
    }
    
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_gpgme_to_error (gerr, err);
        return FALSE;
    }
    
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * SIGN 
 */

static gpgme_key_t
prompt_signer ()
{
    gpgme_error_t gerr = GPG_OK;
    CryptUIKeyset *keyset;
    gpgme_key_t key = NULL;
    gpgme_ctx_t ctx = NULL;
    gchar *signer;
    gchar *id;
    
    keyset = cryptui_keyset_new ("openpgp");
    signer = cryptui_prompt_signer (keyset, _("Choose Signer"));
    if (signer) {
        
        id = cryptui_keyset_key_raw_keyid (keyset, signer);
        g_free (signer);
        
        gerr = gpgme_new (&ctx);
        g_return_val_if_fail (GPG_IS_OK (gerr), NULL);
        
        /* Load up the GPGME secret key */
        gerr = gpgme_get_key (ctx, id, &key, 1);
        g_free (id);
        
        gpgme_release (ctx);
        
        if (!GPG_IS_OK (gerr))
            seahorse_util_handle_gpgme (gerr, _("Couldn't load keys"));
    }

    g_object_unref (keyset);
    return key;
}

static gboolean
sign_start (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata, 
            SeahorsePGPOperation *pop, GError **err)
{
    gpgme_data_t cipher;
    gpgme_error_t gerr;
    gchar *touri;
    
    g_assert (mode->signer);
    
    /* File to encrypt to */
    touri = seahorse_util_add_suffix (uri, SEAHORSE_SIG_SUFFIX, 
                                      _("Choose Signature File Name for '%s'"));
    if (!touri) 
        return FALSE;

    /* Open necessary files, release these with the operation */
    cipher = seahorse_vfs_data_create (touri, SEAHORSE_VFS_WRITE | SEAHORSE_VFS_DELAY, err);
    g_free (touri);
    if (!cipher) 
        return FALSE;
    g_object_set_data_full (G_OBJECT (pop), "cipher-data", cipher, 
                            (GDestroyNotify)gpgme_data_release);
    
    gpgme_set_armor (pop->gctx, seahorse_gconf_get_boolean (ARMOR_KEY));
    gpgme_set_textmode (pop->gctx, FALSE);
    
    /* Start actual signage */
    gpgme_signers_clear (pop->gctx);
    gpgme_signers_add (pop->gctx, mode->signer);
    gerr = gpgme_op_sign_start (pop->gctx, uridata, cipher, GPGME_SIG_MODE_DETACH);
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_gpgme_to_error (gerr, err);
        return FALSE;
    }
    
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * IMPORT 
 */

static gboolean
import_start (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata,
              SeahorsePGPOperation *pop, GError **err)
{
    gpgme_error_t gerr;
    
    /* Start actual import */
    gerr = gpgme_op_import_start (pop->gctx, uridata);
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_gpgme_to_error (gerr, err);
        return FALSE;
    }
    
    return TRUE;
}

static gboolean
import_done (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata,
             SeahorsePGPOperation *pop, GError **err)
{
    gpgme_import_result_t results;
    gpgme_import_status_t import;
    int i;
    
    /* Figure out how many keys were imported */
    results = gpgme_op_import_result (pop->gctx);
    if (results) {
        for (i = 0, import = results->imports; 
             i < results->considered && import; 
             import = import->next) {
            if (GPG_IS_OK (import->result))
                mode->imports++;
        }
    }

    return FALSE;
}

static void
import_show (SeahorseToolMode *mode)
{
    GtkWidget *dlg;
    gchar *t;
    
    /* TODO: This should eventually use libnotify */
    
    if (mode->imports <= 0)
        return;
    
    t = g_strdup_printf (mode->imports == 1 ? _("Imported key") : 
                                              _("Imported %d keys"), mode->imports);
    dlg = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_INFO, GTK_BUTTONS_OK, t);
    g_free (t);
    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
}

/* -----------------------------------------------------------------------------
 * DECRYPT
 */

static gboolean 
decrypt_start (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata,
               SeahorsePGPOperation *pop, GError **err)
{
    gpgme_data_t plain;
    gpgme_error_t gerr;
    gchar *touri;

    /* File to decrypt to */
    touri = seahorse_util_remove_suffix (uri, _("Choose Decrypted File Name for '%s'"));
    if (!touri) 
        return FALSE;

    /* Open necessary files, release these with the operation */
    plain = seahorse_vfs_data_create (touri, SEAHORSE_VFS_WRITE | SEAHORSE_VFS_DELAY, err);
    g_free (touri);
    if (!plain) 
        return FALSE;
    g_object_set_data_full (G_OBJECT (pop), "plain-data", plain, 
                            (GDestroyNotify)gpgme_data_release);
    
    /* Start actual decryption */
    gerr = gpgme_op_decrypt_verify_start (pop->gctx, uridata, plain);
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_gpgme_to_error (gerr, err);
        return FALSE;
    }

    return TRUE;
}

static gboolean
decrypt_done (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata,
              SeahorsePGPOperation *pop, GError **err)
{
    gpgme_verify_result_t status;
    
    status = gpgme_op_verify_result (pop->gctx);
    if (status && status->signatures)
        seahorse_notify_signatures (uri, status);
    
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * VERIFY
 */

static gboolean 
verify_start (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata,
              SeahorsePGPOperation *pop, GError **err)
{
    gpgme_data_t plain;
    gpgme_error_t gerr;
    gchar *original;

    /* File to decrypt to */
    original = seahorse_util_remove_suffix (uri, NULL);
    
    /* The original file doesn't exist, prompt for it */
    if (!seahorse_util_uri_exists (original)) {
           
        GtkWidget *dialog;
        gchar *t;
        
        t = g_strdup_printf (_("Choose Original File for '%s'"), 
                             seahorse_util_uri_get_last (uri));
            
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
        
        seahorse_tool_progress_block (TRUE);
            
        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) 
            original = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

        seahorse_tool_progress_block (FALSE);
        
        gtk_widget_destroy (dialog);
    }

    if (!original) 
        return FALSE;
    
    g_object_set_data_full (G_OBJECT (pop), "original-file", original, g_free);
    
    /* Open necessary files, release with operation */
    plain = seahorse_vfs_data_create (original, SEAHORSE_VFS_READ, err);
    if (!plain)
        return FALSE;
    g_object_set_data_full (G_OBJECT (pop), "plain-data", plain, 
                            (GDestroyNotify)gpgme_data_release);

    /* Start actual verify */
    gerr = gpgme_op_verify_start (pop->gctx, uridata, plain, NULL);
    if (!GPG_IS_OK (gerr)) {
        seahorse_util_gpgme_to_error (gerr, err);
        return FALSE;
    }
    
    return TRUE;
}

static gboolean
verify_done (SeahorseToolMode *mode, const gchar *uri, gpgme_data_t uridata,
             SeahorsePGPOperation *pop, GError **err)
{
    const gchar *orig; 
    gpgme_verify_result_t status;

    status = gpgme_op_verify_result (pop->gctx);
    if (status && status->signatures) {
        
        orig = g_object_get_data (G_OBJECT (pop), "original-file");
        if (!orig)
            orig = uri;
        seahorse_notify_signatures (orig, status);
        
    } else {
        
        /* 
         * TODO: What should happen with multiple files at this point. 
         * The last thing we want to do is cascade a big pile of error
         * dialogs at the user.
         */
        
        g_set_error (err, SEAHORSE_ERROR, -1, _("No valid signatures found"));
        return FALSE;
    }
        
    return TRUE;
}

/* -----------------------------------------------------------------------------
 * MAIN
 */

/* TODO: Temp. Checks to see if any dialogs are open when we're called  
   as a command line app. Once all are gone, closes app */
static gboolean
check_dialogs (gpointer dummy)
{
    if(seahorse_notification_have ())
        return TRUE;

    gtk_main_quit ();
    return FALSE;
}

int
main (int argc, char **argv)
{
    SeahorseToolMode mode;
    GnomeProgram *program = NULL;
    SeahorseContext *sctx;
    gchar **uris = NULL;
    int ret = 0;
    poptContext pctx;
    GValue value = { 0, };


    seahorse_secure_memory_init (65536);
    
    /* We need to drop privileges completely for security */
#if defined(HAVE_SETRESUID) && defined(HAVE_SETRESGID)

    /* Not in header files for all OSs, even where present */
    int setresuid(uid_t ruid, uid_t euid, uid_t suid);
    int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
  
    if (setresuid (getuid (), getuid (), getuid ()) == -1 ||
        setresgid (getgid (), getgid (), getgid ()) == -1)
#else
    if (setuid (getuid ()) == -1 || setgid (getgid ()) == -1)
#endif
        g_error (_("couldn't drop privileges properly"));
    
#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif
    
    /* 
     * In order to keep the progress window responsive we use a model 
     * where two processes communicate with each other. One does the 
     * operations, the other shows the progress window, handles cancel.
     */
    seahorse_tool_progress_init (argc, argv);
    
    /* Main operation process */
    program = gnome_program_init("seahorse-tool", VERSION, LIBGNOMEUI_MODULE, argc, argv,
                                 GNOME_PARAM_POPT_TABLE, options,
                                 GNOME_PARAM_HUMAN_READABLE_NAME, _("File Encryption Tool"),
                                 GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);

    g_value_init (&value, G_TYPE_POINTER);
    g_object_get_property (G_OBJECT (program), GNOME_PARAM_POPT_CONTEXT, &value);
    
    pctx = g_value_get_pointer (&value);
    g_value_unset (&value);

    if (cmd_mode == MODE_NONE) {
        fprintf (stderr, "seahorse-tool: must specify an operation");
        poptPrintHelp (pctx, stdout, 0);
        return 2;
    }

    /* Load up all our arguments */
    uris = read_uri_arguments(pctx);

    if(!uris || !uris[0]) {
        fprintf (stderr, "seahorse-tool: must specify files");
        poptPrintHelp (pctx, stdout, 0);
        return 2;
    }

    /* Insert Icons into Stock */ 
    seahorse_gtkstock_init();
    
    /* Make the default SeahorseContext */
    sctx = seahorse_context_new (SEAHORSE_CONTEXT_APP, 0);
    
    /* The basic settings for the operation */
    memset (&mode, 0, sizeof (mode));
    
    switch (cmd_mode) {
    case MODE_ENCRYPT_SIGN:
    case MODE_ENCRYPT:
        mode.recipients = prompt_recipients (&mode.signer);
        if (mode.recipients) {
            mode.title = _("Encrypting");
            mode.errmsg = _("Couldn't encrypt file: %s");
            mode.startcb = encrypt_sign_start;
            mode.package = TRUE;
        }
        break;
    case MODE_SIGN:
        mode.signer = prompt_signer ();
        if (mode.signer) {
            mode.title = _("Signing");
            mode.errmsg = _("Couldn't sign file: %s");
            mode.startcb = sign_start;
        }
        break;
    case MODE_IMPORT:
        mode.title = _("Importing");
        mode.errmsg = _("Couldn't import keys from file: %s");
        mode.startcb = import_start;
        mode.donecb = import_done;
        mode.imports = 0;
        break;
    case MODE_DECRYPT:
        mode.title = _("Decrypting");
        mode.errmsg = _("Couldn't decrypt file: %s");
        mode.startcb = decrypt_start;
        mode.donecb = decrypt_done;
        break;
    case MODE_VERIFY:
        mode.title = _("Verifying");
        mode.errmsg = _("Couldn't verify file: %s");
        mode.startcb = verify_start;
        mode.donecb = verify_done;
        break;
    default:
        g_assert_not_reached ();
        break;
    };
    
    /* Must at least have a start cb to do something */
    if (mode.startcb) {
        
        ret = seahorse_tool_files_process (&mode, (const gchar**)uris);
    
        /* Any results necessary */
        if (ret == 0) {
            
            switch (cmd_mode) {
            case MODE_IMPORT:
                import_show (&mode);
                break;
            default:
                break;
            };
            
        }
    }
    
    /* TODO: This is temporary code. The actual display of these things should 
       be via the daemon. */
    g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)check_dialogs, NULL, NULL);
    gtk_main ();
    
    if (mode.recipients)
        seahorse_util_free_keys (mode.recipients);
    if (mode.signer)
        gpgmex_key_unref (mode.signer);
    
    g_strfreev (uris);
    
    seahorse_context_destroy (sctx);
        
    if (gnome_vfs_initialized ())
        gnome_vfs_shutdown ();
    
    return ret;
}

