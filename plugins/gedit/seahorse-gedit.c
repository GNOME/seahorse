/*
 * Seahorse
 *
 * Copyright (C) 2004-2005 Nate Nielsen
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <gedit/gedit-plugin.h>
#include <gedit/gedit-debug.h>
#include <gedit/gedit-utils.h>

#include <dbus/dbus-glib-bindings.h>

#include <cryptui.h>
#include "seahorse-gedit.h"

/* -----------------------------------------------------------------------------
 * DECLARATIONS
 */
 
typedef enum {
    SEAHORSE_TEXT_TYPE_NONE,
    SEAHORSE_TEXT_TYPE_KEY,
    SEAHORSE_TEXT_TYPE_MESSAGE,
    SEAHORSE_TEXT_TYPE_SIGNED
} SeahorseTextType;

typedef struct _SeahorsePGPHeader {
    const gchar *header;
    const gchar *footer;
    SeahorseTextType type;
} SeahorsePGPHeader;    

/* Setup in init_crypt */
DBusGConnection *dbus_connection = NULL;
DBusGProxy      *dbus_key_proxy = NULL;
DBusGProxy      *dbus_crypto_proxy = NULL;
CryptUIKeyset   *dbus_keyset = NULL;

/* -----------------------------------------------------------------------------
 * HELPER FUNCTIONS 
 * 
 * These helper functions are for manipulating the GEditDocument, which is 
 * actually a GtkTextView. This is a little more complicated than just calling 
 * gedit_document_* but is needed for for compatibility purposes. (bug# 156368)
 */
 
static gchar *
get_document_chars (GeditDocument *doc, gint start, gint end)
{
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    g_assert (GEDIT_IS_DOCUMENT (doc));
    g_assert (start >= 0);
    g_assert ((end > start) || (end < 0));

    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &start_iter, start);

    if (end < 0)
        gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end_iter);
    else
        gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &end_iter, end);

    return gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (doc), &start_iter, &end_iter, TRUE);
}

static gboolean 
get_document_selection (GeditDocument *doc, gint *start, gint *end)
{
    gboolean ret;
    GtkTextIter iter;
    GtkTextIter sel_bound;

    g_assert (GEDIT_IS_DOCUMENT (doc));

    ret = gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (doc),          
                                                &iter, &sel_bound);

    gtk_text_iter_order (&iter, &sel_bound);    

    if (start != NULL)
        *start = gtk_text_iter_get_offset (&iter); 

    if (end != NULL)
        *end = gtk_text_iter_get_offset (&sel_bound); 

    return ret;
}

static void
set_document_selection (GeditDocument *doc, gint start, gint end)
{
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    g_assert (GEDIT_IS_DOCUMENT (doc));
    g_assert (start >= 0);
    g_assert ((end > start) || (end < 0));

    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &start_iter, start);

    if (end < 0)
        gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end_iter);
    else
        gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &end_iter, end);

    gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (doc), &end_iter);
    gtk_text_buffer_move_mark_by_name (GTK_TEXT_BUFFER (doc),
                                        "selection_bound", &start_iter);
}

static void
replace_selected_text (GeditDocument *doc, const gchar *replace)
{
    GtkTextIter iter;
    GtkTextIter sel_bound;

    g_assert (GEDIT_IS_DOCUMENT (doc));
    g_assert (replace != NULL);

    if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (doc),           
                                                  &iter, &sel_bound)) {
        SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "There is no selected text");
        return;
    }

    gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (doc));

    gtk_text_buffer_delete_selection (GTK_TEXT_BUFFER (doc), FALSE, TRUE);

    gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),            
                   &iter, gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (doc)));

    g_printerr ("%s\n", replace);

    if (*replace != '\0')
        gtk_text_buffer_insert (GTK_TEXT_BUFFER (doc), &iter,
                 replace, strlen (replace));

    gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (doc));
}

SeahorseTextType    
detect_text_type (const gchar *text, gint len, const gchar **start, const gchar **end)
{
    const SeahorsePGPHeader *header;
    const gchar *pos = NULL;
    const gchar *t;
    int i;
    
    static const SeahorsePGPHeader seahorse_pgp_headers[] = {
        { 
            "-----BEGIN PGP MESSAGE-----", 
            "-----END PGP MESSAGE-----", 
            SEAHORSE_TEXT_TYPE_MESSAGE 
        }, 
        {
            "-----BEGIN PGP SIGNED MESSAGE-----",
            "-----END PGP SIGNATURE-----",
            SEAHORSE_TEXT_TYPE_SIGNED
        }, 
        {
            "-----BEGIN PGP PUBLIC KEY BLOCK-----",
            "-----END PGP PUBLIC KEY BLOCK-----",
            SEAHORSE_TEXT_TYPE_KEY
        }, 
        {
            "-----BEGIN PGP PRIVATE KEY BLOCK-----",
            "-----END PGP PRIVATE KEY BLOCK-----",
            SEAHORSE_TEXT_TYPE_KEY
        }
    };
    
    if (len == -1)
        len = strlen (text);
    
    /* Find the first of the headers */
    for (i = 0; i < (sizeof (seahorse_pgp_headers) / sizeof (seahorse_pgp_headers[0])); i++) {
        t = g_strstr_len (text, len, seahorse_pgp_headers[i].header);
        if (t != NULL) {
            if (pos == NULL || (t < pos)) {
                header = &(seahorse_pgp_headers[i]);
                pos = t;
            }
        }
    }
    
    if (pos != NULL) {
        
        if (start)
            *start = pos;
        
        /* Find the end of that block */
        t = g_strstr_len (pos, len - (pos - text), header->footer);
        if (t != NULL && end)
            *end = t + strlen(header->footer);
        else if (end)
            *end = NULL;
            
        return header->type;
    }
    
    return SEAHORSE_TEXT_TYPE_NONE;
}

void
seahorse_gedit_show_error (const gchar *heading, GError *error)
{
    GtkWidget *dlg;
    gchar *t;
    
    g_assert (heading);
    g_assert (error);
    
    t = g_strconcat("<big><b>", heading, "</b></big>\n\n", 
                    error->message ? error->message : "", NULL);
    
    dlg = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_CLOSE, t);
    g_free (t);
    
    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_widget_destroy (dlg);
    
    g_clear_error (&error);
}

/* -----------------------------------------------------------------------------
 * INITIALIZATION / CLEANUP 
 */

static gboolean
init_crypt ()
{
    GError *error = NULL;
    
    if (!dbus_connection) {
        dbus_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (!dbus_connection) {
            seahorse_gedit_show_error (_("Couldn't connect to seahorse-daemon"), error);
            return FALSE;
        }

        dbus_key_proxy = dbus_g_proxy_new_for_name (dbus_connection, "org.gnome.seahorse",
                                               "/org/gnome/seahorse/keys",
                                               "org.gnome.seahorse.KeyService");
        
        dbus_crypto_proxy = dbus_g_proxy_new_for_name (dbus_connection, "org.gnome.seahorse",
                                               "/org/gnome/seahorse/crypto",
                                               "org.gnome.seahorse.CryptoService");
        
        dbus_keyset = cryptui_keyset_new ("openpgp", TRUE);
    }
    
    return TRUE;
}

void
seahorse_gedit_cleanup ()
{
    if (dbus_key_proxy)
        g_object_unref (dbus_key_proxy);
    dbus_key_proxy = NULL;
    
    if (dbus_crypto_proxy)
        g_object_unref (dbus_crypto_proxy);
    dbus_crypto_proxy = NULL;
    
    if (dbus_connection)
        dbus_g_connection_unref (dbus_connection);
    dbus_connection = NULL;
}

/* -----------------------------------------------------------------------------
 * CRYPT CALLBACKS
 */

/* Callback for encrypt menu item */
void
seahorse_gedit_encrypt (GeditDocument *doc)
{
    GError *error = NULL;
    gchar *enctext = NULL;
    gchar *buffer;
    gchar **keys;
    gchar *signer;
    gint start, end;
    gboolean ret;
    
    if (!init_crypt ())
        return;

    g_return_if_fail (doc != NULL);

    /* 
     * We get the selection bounds before getting recipients.
     * This is because in some cases the selection is removed
     * once we start messing with recipients.
     */
    if (!get_document_selection (doc, &start, &end)) {
        start = 0;
        end = -1;
    }
    
    /* Get the recipient list */
    SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "getting recipients");
    keys = cryptui_prompt_recipients (dbus_keyset, _("Choose Recipient Keys"), &signer);

    /* User may have cancelled */
    if (keys && *keys) {

        /* Get the document text */
        buffer = get_document_chars (doc, start, end);
        /* Encrypt it */
        SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "encrypting text");
        
        ret = dbus_g_proxy_call (dbus_crypto_proxy, "EncryptText", &error, 
                                G_TYPE_STRV, keys, 
                                G_TYPE_STRING, signer, 
                                G_TYPE_INT, 0,
                                G_TYPE_STRING, buffer,
                                G_TYPE_INVALID,
                                G_TYPE_STRING, &enctext,
                                G_TYPE_INVALID);
        
        if (ret) {
            set_document_selection (doc, start, end);
            replace_selected_text (doc, enctext);
            seahorse_gedit_flash (_("Encrypted text"));
            g_free (enctext);
        } else {
            seahorse_gedit_show_error (_("Couldn't encrypt text"), error);
        }
        
        /* And finish up */
        g_strfreev (keys);
        g_free (signer);
        g_free (buffer);
    }
}

/* When we try to 'decrypt' a key, this gets called */
static guint
import_keys (const gchar *text)
{
    GError *error = NULL;
    gchar **keys, **k;
    int nkeys = 0;
    gboolean ret;
    
    if (!init_crypt ())
        return 0;
    
    ret = dbus_g_proxy_call (dbus_key_proxy, "ImportKeys", &error,
                             G_TYPE_STRING, "openpgp",
                             G_TYPE_STRING, text,
                             G_TYPE_INVALID,
                             G_TYPE_STRV, &keys,
                             G_TYPE_INVALID);
    
    if (!ret) {
        seahorse_gedit_show_error (_("Couldn't import keys"), error);
        return 0;
    }
    
    for (k = keys, nkeys = 0; *k; k++)
        nkeys++;
    g_strfreev (keys);
    
    if (!nkeys) 
        seahorse_gedit_flash (_("Keys found but not imported"));
    
    return nkeys;
}

/* Decrypt an encrypted message */
static gchar*
decrypt_text (const gchar *text)
{
    GError *error = NULL;
    gchar *rawtext = NULL;
    gchar *signer;
    gboolean ret;
    
    if (!init_crypt ())
        return NULL;
    
g_printerr ("%s\n", text);    
    
    ret = dbus_g_proxy_call (dbus_crypto_proxy, "DecryptText", &error,
                             G_TYPE_STRING, "openpgp",
                             G_TYPE_INT, 0,
                             G_TYPE_STRING, text,
                             G_TYPE_INVALID, 
                             G_TYPE_STRING, &rawtext,
                             G_TYPE_STRING, &signer,
                             G_TYPE_INVALID);
    
    if (ret) {
        /* Not interested in the signer */
        g_free (signer);
        return rawtext;
        
    } else {
        seahorse_gedit_show_error (_("Couldn't decrypt text"), error);
        return NULL;
    }
}

/* Verify a signed message */
static gchar*
verify_text (const gchar * text)
{
    GError *error = NULL;
    gchar *rawtext = NULL;
    gchar *signer;
    gboolean ret;
    
    if (!init_crypt ())
        return NULL;

    ret = dbus_g_proxy_call (dbus_crypto_proxy, "VerifyText", &error,
                             G_TYPE_STRING, "openpgp",
                             G_TYPE_INT, 0,
                             G_TYPE_STRING, text,
                             G_TYPE_INVALID, 
                             G_TYPE_STRING, &rawtext,
                             G_TYPE_STRING, &signer,
                             G_TYPE_INVALID);
    
    if (ret) {
        /* Not interested in the signer */
        g_free (signer);
        return rawtext;
        
    } else {
        seahorse_gedit_show_error (_("Couldn't verify text"), error);
        return NULL;
    }
}

/* Called for the decrypt menu item */
void
seahorse_gedit_decrypt (GeditDocument *doc)
{
    gchar *buffer;              /* The text selected */
    gint sel_start;             /* The end of the whole deal */
    gint sel_end;               /* The beginning of the whole deal */

    SeahorseTextType type;      /* Type of the current block */
    guint block_len;            /* Length of a given block */
    guint block_pos;            /* Position (in the doc) of a given block */
    gchar *last;                /* Pointer to end of last block */
    const gchar *start;         /* Pointer to start of the block */
    const gchar *end;           /* Pointer to end of the block */

    gchar *rawtext;             /* Replacement text */
    guint raw_len;              /* Length of replacement text */
    
    guint blocks = 0;           /* Number of blocks processed */
    guint keys = 0;             /* Number of keys imported */

    g_return_if_fail (doc != NULL);

    if (!get_document_selection (doc, &sel_start, &sel_end)) {
        sel_start = 0;
        sel_end = gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc));
    }

    buffer = get_document_chars (doc, sel_start, sel_end);
    

    last = buffer;
    block_pos = sel_start;
    
    for (;;) {

        /* Try to figure out what it contains */
        type = detect_text_type (last, -1, &start, &end);
        SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "detected type: %d", type);
        
        if (type == SEAHORSE_TEXT_TYPE_NONE) {
            if (blocks == 0) 
                gedit_warning (GTK_WINDOW (seahorse_gedit_active_window ()),
                               _("No encrypted or signed text is selected."));
            break;
        }
        
        g_assert (start >= last);

        /* Note that blocks always have at least one char between
         * them. Usually this is a line ending */
        if (end != NULL)
            *((gchar *) end) = 0;
        else
            end = last + strlen(last);
            
        block_pos += start - last;
        block_len = end - start;
        
        SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "block (pos: %d, len %d)", block_pos, block_len);
        
        switch (type) {

        /* A key, import it */
        case SEAHORSE_TEXT_TYPE_KEY:
            SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "importing key");
            keys += import_keys (start);
            break;

        /* A message decrypt it */
        case SEAHORSE_TEXT_TYPE_MESSAGE:
            SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "decrypting message");
            rawtext = decrypt_text (start);
            seahorse_gedit_flash (_("Decrypted text"));
            break;

        /* A message verify it */
        case SEAHORSE_TEXT_TYPE_SIGNED:
            SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "verifying message");
            rawtext = verify_text (start);
            seahorse_gedit_flash (_("Verified text"));
            break;

        default:
            g_assert_not_reached ();
            break;
        };
        
        /* We got replacement text */
        if (rawtext) {

            /* Replace the current block */
            set_document_selection (doc, block_pos, block_pos + block_len);
            
            /* With the new text */
            replace_selected_text (doc, rawtext);

            /* And update our book keeping */
            raw_len = strlen (rawtext);
            block_pos += raw_len + 1;

            SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "raw (pos: %d, len %d)", block_pos, raw_len);
            g_free (rawtext);
            rawtext = NULL;
            
        /* No replacement text, skip ahead */
        } else {
            block_pos += block_len + 1;
        }
        
        last = (gchar*)end + 1;
        blocks++;
    }
    
    if (keys > 0)
        seahorse_gedit_flash (ngettext ("Imported %d key", "Imported %d keys", keys), keys);

    g_free (buffer);
}

/* Callback for the sign menu item */
void
seahorse_gedit_sign (GeditDocument *doc)
{
    GError *error = NULL;
    gchar *enctext = NULL;
    gchar *buffer;
    gchar *signer;
    gint start, end;
    gboolean ret;

    if (!init_crypt ())
        return;

    g_return_if_fail (doc != NULL);

    if (!get_document_selection (doc, &start, &end)) {
        start = 0;
        end = -1;
    }

    /* Get the document text */
    buffer = get_document_chars (doc, start, end);

    signer = cryptui_prompt_signer (dbus_keyset, _("Choose Key to Sign with"));
    
    /* User may have cancelled */
    if (signer != NULL) {

        /* Get the document text */
        buffer = get_document_chars (doc, start, end);

        /* Sign it */
        SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "signing text");
        
        ret = dbus_g_proxy_call (dbus_crypto_proxy, "SignText", &error, 
                                G_TYPE_STRING, signer, 
                                G_TYPE_INT, 0,
                                G_TYPE_STRING, buffer,
                                G_TYPE_INVALID,
                                G_TYPE_STRING, &enctext,
                                G_TYPE_INVALID);
        
        if (ret) {
            set_document_selection (doc, start, end);
            replace_selected_text (doc, enctext);
            seahorse_gedit_flash (_("Signed text"));
            g_free (enctext);
        } else {
            seahorse_gedit_show_error (_("Couldn't sign text"), error);
        }
        
        /* And finish up */
        g_free (signer);
        g_free (buffer);
    }
}

