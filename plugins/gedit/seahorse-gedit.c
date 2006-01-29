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

#include <libgnome/gnome-i18n.h>

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <gedit/gedit-plugin.h>
#include <gedit/gedit-debug.h>
#include <gedit/gedit-utils.h>

#include "seahorse-gedit.h"
#include "seahorse-gpgmex.h"
#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-libdialogs.h"
#include "seahorse-op.h"
#include "seahorse-widget.h"

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

    g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);
    g_return_val_if_fail (start >= 0, NULL);
    g_return_val_if_fail ((end > start) || (end < 0), NULL);

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

    g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

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

    g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
    g_return_if_fail (start >= 0);
    g_return_if_fail ((end > start) || (end < 0));

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

    g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
    g_return_if_fail (replace != NULL);

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

/* -----------------------------------------------------------------------------
 * CRYPT CALLBACKS
 */

/* Callback for encrypt menu item */
void
seahorse_gedit_encrypt (SeahorseContext *sctx, GeditDocument *doc)
{
    SeahorseKeyPair *signer = NULL;
    gpgme_error_t err = GPG_OK;
    gchar *enctext = NULL;
    gchar *buffer;
    GList *keys;
    gint start;
    gint end;

    g_assert (SEAHORSE_IS_CONTEXT (sctx));
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
    keys = seahorse_recipients_get (sctx, &signer);

    /* User may have cancelled */
    if (g_list_length(keys) == 0)
        return;

    /* Get the document text */
    buffer = get_document_chars (doc, start, end);    

    /* Encrypt it */
    SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "encrypting text");
    if (signer == NULL)
        enctext = seahorse_op_encrypt_text (keys, buffer, &err);
    else
        enctext = seahorse_op_encrypt_sign_text (keys, signer, buffer, &err);

    g_list_free (keys);
    g_free (buffer);

    if (!GPG_IS_OK (err)) {
        g_assert (!enctext);
        seahorse_util_handle_gpgme (err, _("Couldn't encrypt text"));
        return;
    }

    /* And finish up */
    set_document_selection (doc, start, end);
    replace_selected_text (doc, enctext);
    seahorse_gedit_flash (_("Encrypted text"));
    g_free (enctext);
}

/* When we try to 'decrypt' a key, this gets called */
static guint
import_keys (SeahorseContext *sctx, const gchar *text)
{
    SeahorseKeySource *sksrc;
    GError *err = NULL;
    gint keys;

    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 0);
    
    keys = seahorse_op_import_text (sksrc, text, &err);

    if (keys < 0) {
        seahorse_util_handle_error (err, _("Couldn't import keys"));
        return 0;
    } else if (keys == 0) {    
        seahorse_gedit_flash (_("Keys found but not imported"));
        return 0;
    } else {
        return keys;
    }
}

/* Decrypt an encrypted message */
static gchar*
decrypt_text (SeahorseContext *sctx, const gchar * text, gpgme_verify_result_t *status)
{
    SeahorseKeySource *sksrc;
    gpgme_error_t err;
    gchar *rawtext = NULL;
    
    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 0);

    rawtext = seahorse_op_decrypt_verify_text (sksrc, text, status, &err);

    if (!GPG_IS_OK (err)) {
        seahorse_util_handle_gpgme (err, _("Couldn't decrypt text"));
        return NULL;
    }

    return rawtext;
}

/* Verify a signed message */
static gchar*
verify_text (SeahorseContext *sctx, const gchar *text, gpgme_verify_result_t *status)
{
    SeahorseKeySource *sksrc;
    gpgme_error_t err;
    gchar *rawtext = NULL;

    sksrc = seahorse_context_get_key_source (sctx);
    g_return_val_if_fail (sksrc != NULL, 0);    

    rawtext = seahorse_op_verify_text (sksrc, text, status, &err);

    if (!GPG_IS_OK (err)) {
        seahorse_util_handle_gpgme (err, _("Couldn't decrypt text"));
        return NULL;
    }

    return rawtext;
}

/* Called for the decrypt menu item */
void
seahorse_gedit_decrypt (SeahorseContext *sctx, GeditDocument *doc)
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

    SeahorseWidget *sigs = NULL;    /* Signature window */
    gpgme_verify_result_t status;   /* Signature status of last operation */
    
    g_assert (SEAHORSE_IS_CONTEXT (sctx));
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
        type = seahorse_util_detect_text (last, -1, &start, &end);
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
        status = NULL;
        
        switch (type) {

        /* A key, import it */
        case SEAHORSE_TEXT_TYPE_KEY:
            SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "importing key");
            keys += import_keys (sctx, start);
            break;

        /* A message decrypt it */
        case SEAHORSE_TEXT_TYPE_MESSAGE:
            SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "decrypting message");
            rawtext = decrypt_text (sctx, start, &status);
            seahorse_gedit_flash (_("Decrypted text"));
            break;

        /* A message verify it */
        case SEAHORSE_TEXT_TYPE_SIGNED:
            SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "verifying message");
            rawtext = verify_text (sctx, start, &status);
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
            
            if(status && status->signatures) {
                gchar *t;
                
                if(!sigs) 
                    sigs = seahorse_signatures_new (sctx);
                    
                t = g_strdup_printf (_("Block %d"), blocks + 1);
                seahorse_signatures_add (sctx, sigs, t, status);
                g_free (t);
            }
            
        /* No replacement text, skip ahead */
        } else {
            block_pos += block_len + 1;
        }
        
        last = (gchar*)end + 1;
        blocks++;
    }
    
    if (keys > 0)
        seahorse_gedit_flash (ngettext("Imported %d key", "Imported %d keys", keys), keys);

    g_free (buffer);
    
    if (sigs)
        seahorse_signatures_run (sctx, sigs);
}

/* Callback for the sign menu item */
void
seahorse_gedit_sign (SeahorseContext *sctx, GeditDocument *doc)
{
    gpgme_error_t err = GPG_OK;
    SeahorseKeyPair *signer;
    gchar *enctext = NULL;
    gchar *buffer;
    gint start;
    gint end;

    g_assert (SEAHORSE_IS_CONTEXT (sctx));
    g_return_if_fail (doc != NULL);

    if (!get_document_selection (doc, &start, &end)) {
        start = 0;
        end = -1;
    }

    /* Get the document text */
    buffer = get_document_chars (doc, start, end);

    signer = seahorse_signer_get (sctx);
    if (signer == NULL)
        return;

    /* Perform the signing */
    SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "signing text");
    enctext = seahorse_op_sign_text (signer, buffer, &err);
    g_free (buffer);

    if (!GPG_IS_OK (err)) {
        g_assert (!enctext);
        seahorse_util_handle_gpgme (err, _("Couldn't sign text"));
        return;
    }

    /* Finish up */
    set_document_selection (doc, start, end);
    replace_selected_text (doc, enctext);
    seahorse_gedit_flash (_("Signed text"));
    g_free (enctext);
}
