/*
 * Seahorse
 *
 * Copyright (C) 2004 Nate Nielsen
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
#include <gedit/gedit-file.h>
#include <gedit/gedit-debug.h>
#include <gedit/gedit-menus.h>
#include <gedit/gedit-utils.h>

#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-libdialogs.h"
#include "seahorse-op.h"
#include "seahorse-widget.h"

#define MENU_ENC_ITEM_LABEL      N_("_Encrypt...")
#define MENU_ENC_ITEM_PATH       "/menu/Edit/EditOps_6/"
#define MENU_ENC_ITEM_NAME       "Encrypt"
#define MENU_ENC_ITEM_TIP        N_("PGP Encrypt the selected text")

#define MENU_DEC_ITEM_LABEL      N_("Decr_ypt/Verify")
#define MENU_DEC_ITEM_PATH       "/menu/Edit/EditOps_6/"
#define MENU_DEC_ITEM_NAME       "Decrypt"
#define MENU_DEC_ITEM_TIP        N_("Decrypt and/or Verify PGP text")

#define MENU_SIGN_ITEM_LABEL      N_("Sig_n...")
#define MENU_SIGN_ITEM_PATH       "/menu/Edit/EditOps_6/"
#define MENU_SIGN_ITEM_NAME       "Sign"
#define MENU_SIGN_ITEM_TIP        N_("PGP Sign the selected text")

static SeahorseContext *sctx = NULL;

/* Callback for encrypt menu item */
static void
encrypt_cb (BonoboUIComponent * uic, gpointer user_data,
            const gchar * verbname)
{
    GeditView *view = gedit_get_active_view ();
    GeditDocument *doc;
    gpgme_error_t err = GPG_OK;
    gpgme_key_t *recips;
    gchar *enctext = NULL;
    gchar *buffer;
    gint start;
    gint end;

    gedit_debug (DEBUG_PLUGINS, "");
    g_assert (SEAHORSE_IS_CONTEXT (sctx));

    g_return_if_fail (view);

    /* Get the recipient list */
    gedit_debug (DEBUG_PLUGINS, "getting recipients");
    recips = seahorse_recipients_get (sctx);

    /* User may have cancelled */
    if (recips == NULL)
        return;

    /* Get the text */
    doc = gedit_view_get_document (view);
    buffer = gedit_document_get_chars (doc, 0, -1);

    if (gedit_document_get_selection (doc, &start, &end)) {
        buffer = gedit_document_get_chars (doc, start, end);
    } else {
        buffer = gedit_document_get_chars (doc, 0, -1);
    }

    /* Encrypt it */
    gedit_debug (DEBUG_PLUGINS, "encrypting text");
    enctext = seahorse_op_encrypt_text (sctx, buffer, recips, &err);

    seahorse_util_free_keys (recips);
    g_free (buffer);

    if (!GPG_IS_OK (err)) {
        g_assert (!enctext);
        seahorse_util_handle_error (err, _("Couldn't encrypt text"));
        return;
    }

    /* And finish up */
    gedit_document_set_selection (doc, start, end);
    gedit_document_replace_selected_text (doc, enctext);
    gedit_utils_flash (_("Encrypted text"));
    g_free (enctext);
}

/* When we try to 'decrypt' a key, this gets called */
static guint
import_keys (const gchar * text)
{
    gpgme_error_t err;
    gint keys;

    keys = seahorse_op_import_text (sctx, text, &err);

    if (!GPG_IS_OK (err)) {
        seahorse_util_handle_error (err, _("Couldn't import keys"));
        return 0;
    }

    if (keys == 0) {    
        gedit_utils_flash (_("Keys found but not imported"));
        return 0;
    } else {
        return keys;
    }
}

/* Decrypt an encrypted message */
static gchar*
decrypt_text (const gchar * text, gpgme_verify_result_t *status)
{
    gpgme_error_t err;
    gchar *rawtext = NULL;

    rawtext = seahorse_op_decrypt_verify_text (sctx, text, status, &err);

    if (!GPG_IS_OK (err)) {
        seahorse_util_handle_error (err, _("Couldn't decrypt text"));
        return NULL;
    }

    return rawtext;
}

/* Verify a signed message */
static gchar*
verify_text (const gchar * text, gpgme_verify_result_t *status)
{
    gpgme_error_t err;
    gchar *rawtext = NULL;

    rawtext = seahorse_op_verify_text (sctx, text, status, &err);

    if (!GPG_IS_OK (err)) {
        seahorse_util_handle_error (err, _("Couldn't decrypt text"));
        return NULL;
    }

    return rawtext;
}

/* Called for the decrypt menu item */
static void
decrypt_cb (BonoboUIComponent * uic, gpointer user_data,
            const gchar * verbname)
{
    GeditView *view = gedit_get_active_view ();
    GeditDocument *doc;
    
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
    
    gedit_debug (DEBUG_PLUGINS, "");
    g_assert (SEAHORSE_IS_CONTEXT (sctx));

    g_return_if_fail (view);

    /* Get the document text */
    doc = gedit_view_get_document (view);
    buffer = gedit_document_get_chars (doc, 0, -1);

    if (!gedit_document_get_selection (doc, &sel_start, &sel_end)) {
        sel_start = 0;
        sel_end = gedit_document_get_char_count (doc);
    }

    buffer = gedit_document_get_chars (doc, sel_start, sel_end);
    

    last = buffer;
    block_pos = sel_start;
    
    for (;;) {

        /* Try to figure out what it contains */
        type = seahorse_util_detect_text (last, -1, &start, &end);
        gedit_debug (DEBUG_PLUGINS, "detected type: %d", type);
        
        if (type == SEAHORSE_TEXT_TYPE_NONE) {
            if (blocks == 0) 
                gedit_warning (GTK_WINDOW (gedit_get_active_window ()),
                           _("No PGP encrypted or signed text is selected."));
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
        
        gedit_debug (DEBUG_PLUGINS, "block (pos: %d, len %d)", block_pos, block_len);
        status = NULL;
        
        switch (type) {

        /* A key, import it */
        case SEAHORSE_TEXT_TYPE_KEY:
            gedit_debug (DEBUG_PLUGINS, "importing key");
            keys += import_keys (start);
            break;

        /* A message decrypt it */
        case SEAHORSE_TEXT_TYPE_MESSAGE:
            gedit_debug (DEBUG_PLUGINS, "decrypting message");
            rawtext = decrypt_text (start, &status);
            gedit_utils_flash (_("Decrypted text"));
            break;

        /* A message verify it */
        case SEAHORSE_TEXT_TYPE_SIGNED:
            gedit_debug (DEBUG_PLUGINS, "verifying message");
            rawtext = verify_text (start, &status);
            gedit_utils_flash (_("Verified text"));
            break;

        default:
            g_assert_not_reached ();
            break;
        };
        
        /* We got replacement text */
        if (rawtext) {

            /* Replace the current block */
            gedit_document_set_selection (doc, block_pos, block_pos + block_len);
            
            /* With the new text */
            gedit_document_replace_selected_text (doc, rawtext);

            /* And update our book keeping */
            raw_len = strlen (rawtext);
            block_pos += raw_len + 1;

            gedit_debug (DEBUG_PLUGINS, "raw (pos: %d, len %d)", block_pos, raw_len);
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
        gedit_utils_flash_va (ngettext("Imported %d key", "Imported %d keys", keys), keys);

    g_free (buffer);
    
    if (sigs)
        seahorse_signatures_run (sctx, sigs);
}

/* Callback for the sign menu item */
static void
sign_cb (BonoboUIComponent * uic, gpointer user_data,
         const gchar * verbname)
{
    GeditView *view = gedit_get_active_view ();
    GeditDocument *doc;
    gpgme_error_t err = GPG_OK;
    gchar *enctext = NULL;
    gchar *buffer;
    gint start;
    gint end;

    gedit_debug (DEBUG_PLUGINS, "");
    g_assert (SEAHORSE_IS_CONTEXT (sctx));

    g_return_if_fail (view);

    /* Get the document text */
    doc = gedit_view_get_document (view);
    buffer = gedit_document_get_chars (doc, 0, -1);

    if (!gedit_document_get_selection (doc, &start, &end)) {
        start = 0;
        end = -1;
    }

    buffer = gedit_document_get_chars (doc, start, end);

    /* Perform the signing */
    gedit_debug (DEBUG_PLUGINS, "signing text");
    enctext = seahorse_op_sign_text (sctx, buffer, &err);
    g_free (buffer);

    if (!GPG_IS_OK (err)) {
        g_assert (!enctext);
        seahorse_util_handle_error (err, _("Couldn't sign text"));
        return;
    }

    /* Finish up */
    gedit_document_set_selection (doc, start, end);
    gedit_document_replace_selected_text (doc, enctext);
    gedit_utils_flash (_("Signed text"));
    g_free (enctext);
}

/* Called by gedit when time to update the UI */
G_MODULE_EXPORT GeditPluginState
update_ui (GeditPlugin * plugin, BonoboWindow * window)
{
    BonoboUIComponent *uic;
    GeditDocument *doc;
    gboolean sensitive;

    g_return_val_if_fail (window != NULL, PLUGIN_ERROR);

    uic = gedit_get_ui_component_from_window (window);
    doc = gedit_get_active_document ();

    sensitive = (doc && gedit_document_get_char_count (doc) > 0);

    gedit_debug (DEBUG_PLUGINS, "updating UI");
    gedit_menus_set_verb_sensitive (uic, "/commands/" MENU_DEC_ITEM_NAME,
                                    sensitive);
    gedit_menus_set_verb_sensitive (uic, "/commands/" MENU_SIGN_ITEM_NAME,
                                    sensitive);
    gedit_menus_set_verb_sensitive (uic, "/commands/" MENU_ENC_ITEM_NAME,
                                    sensitive);
    return PLUGIN_OK;
}

/* Called once by gedit when the plugin is activated */
G_MODULE_EXPORT GeditPluginState
activate (GeditPlugin * pd)
{
    GList *top_windows;
    gedit_debug (DEBUG_PLUGINS, "adding menu items");

    top_windows = gedit_get_top_windows ();
    g_return_val_if_fail (top_windows != NULL, PLUGIN_ERROR);

    while (top_windows) {
        gedit_menus_add_menu_item (BONOBO_WINDOW (top_windows->data),
                                   MENU_ENC_ITEM_PATH, MENU_ENC_ITEM_NAME,
                                   MENU_ENC_ITEM_LABEL, MENU_ENC_ITEM_TIP,
                                   NULL, encrypt_cb);

        gedit_menus_add_menu_item (BONOBO_WINDOW (top_windows->data),
                                   MENU_SIGN_ITEM_PATH,
                                   MENU_SIGN_ITEM_NAME,
                                   MENU_SIGN_ITEM_LABEL,
                                   MENU_SIGN_ITEM_TIP, NULL, sign_cb);

        gedit_menus_add_menu_item (BONOBO_WINDOW (top_windows->data),
                                   MENU_DEC_ITEM_PATH, MENU_DEC_ITEM_NAME,
                                   MENU_DEC_ITEM_LABEL, MENU_DEC_ITEM_TIP,
                                   NULL, decrypt_cb);

        pd->update_ui (pd, BONOBO_WINDOW (top_windows->data));

        top_windows = g_list_next (top_windows);
    }

    return PLUGIN_OK;
}

/* Called once by gedit when the plugin goes away */
G_MODULE_EXPORT GeditPluginState
deactivate (GeditPlugin * plugin)
{
    gedit_debug (DEBUG_PLUGINS, "removing menu items");
    gedit_menus_remove_menu_item_all (MENU_ENC_ITEM_PATH,
                                      MENU_ENC_ITEM_NAME);
    gedit_menus_remove_menu_item_all (MENU_SIGN_ITEM_PATH,
                                      MENU_SIGN_ITEM_NAME);
    gedit_menus_remove_menu_item_all (MENU_DEC_ITEM_PATH,
                                      MENU_DEC_ITEM_NAME);
    return PLUGIN_OK;
}

/* Called once by gedit when the plugin is destroyed */
G_MODULE_EXPORT GeditPluginState
destroy (GeditPlugin * plugin)
{
    gedit_debug (DEBUG_PLUGINS, "destroy");

    if (sctx && SEAHORSE_IS_CONTEXT (sctx))
        seahorse_context_destroy (sctx);

    sctx = NULL;
    plugin->private_data = NULL;
    return PLUGIN_OK;
}

/* Called first by gedit at program startup */
G_MODULE_EXPORT GeditPluginState
init (GeditPlugin * plugin)
{
    gedit_debug (DEBUG_PLUGINS, "inited");

    sctx = seahorse_context_new ();
    plugin->private_data = sctx;

    return PLUGIN_OK;
}
