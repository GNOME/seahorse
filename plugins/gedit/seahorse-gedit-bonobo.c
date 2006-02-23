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
#include <gedit/gedit-menus.h>

#include "seahorse-context.h"
#include "seahorse-gedit.h"

#define MENU_ENC_ITEM_LABEL      N_("_Encrypt...")
#define MENU_ENC_ITEM_PATH       "/menu/Edit/EditOps_6/"
#define MENU_ENC_ITEM_NAME       "Encrypt"
#define MENU_ENC_ITEM_TIP        N_("Encrypt the selected text")

#define MENU_DEC_ITEM_LABEL      N_("Decr_ypt/Verify")
#define MENU_DEC_ITEM_PATH       "/menu/Edit/EditOps_6/"
#define MENU_DEC_ITEM_NAME       "Decrypt"
#define MENU_DEC_ITEM_TIP        N_("Decrypt and/or Verify text")

#define MENU_SIGN_ITEM_LABEL      N_("Sig_n...")
#define MENU_SIGN_ITEM_PATH       "/menu/Edit/EditOps_6/"
#define MENU_SIGN_ITEM_NAME       "Sign"
#define MENU_SIGN_ITEM_TIP        N_("Sign the selected text")

/* The global plugin context */
static SeahorseContext *sctx = NULL;

/* -----------------------------------------------------------------------------
 * CRYPT CALLBACKS
 */

static void
init_context ()
{
    SeahorseOperation *op;
    
    if (!sctx) {
        sctx = seahorse_context_new (TRUE, SKEY_PGP);
        op = seahorse_context_load_local_keys (sctx);
        
        /* The op frees itself */
        g_object_unref (op);
    }
}

/* Callback for encrypt menu item */
static void
encrypt_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
    init_context ();
    seahorse_gedit_encrypt (sctx, gedit_get_active_document ());
}

/* Called for the decrypt menu item */
static void
decrypt_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
    init_context ();
    seahorse_gedit_decrypt (sctx, gedit_get_active_document ());
}

/* Callback for the sign menu item */
static void
sign_cb (BonoboUIComponent *uic, gpointer user_data, const gchar *verbname)
{
    init_context ();
    seahorse_gedit_sign (sctx, gedit_get_active_document ());
}

/* -----------------------------------------------------------------------------
 * UI STUFF
 */

/* Called by gedit when time to update the UI */
G_MODULE_EXPORT GeditPluginState
update_ui (GeditPlugin *plugin, BonoboWindow *window)
{
    BonoboUIComponent *uic;
    GeditDocument *doc;
    gboolean sensitive;

    g_return_val_if_fail (window != NULL, PLUGIN_ERROR);

    uic = gedit_get_ui_component_from_window (window);
    doc = gedit_get_active_document ();

    sensitive = (doc && gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)) > 0);

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

/* -----------------------------------------------------------------------------
 * PLUGIN BASICS
 */

/* Called once by gedit when the plugin is destroyed */
G_MODULE_EXPORT GeditPluginState
destroy (GeditPlugin *plugin)
{
    gedit_debug (DEBUG_PLUGINS, "seahorse plugin destroyed");

    if (sctx && SEAHORSE_IS_CONTEXT (sctx))
        seahorse_context_destroy (sctx);

    sctx = NULL;
    plugin->private_data = NULL;
    return PLUGIN_OK;
}

/* Called first by gedit at program startup */
G_MODULE_EXPORT GeditPluginState
init (GeditPlugin *plugin)
{
    gedit_debug (DEBUG_PLUGINS, "seahorse plugin inited");

    sctx = NULL;
    return PLUGIN_OK;
}

/* -----------------------------------------------------------------------------
 * HELPERS
 */

void        
seahorse_gedit_flash (const gchar *format, ...)
{
    va_list va;
    gchar *msg; 

    va_start(va, format);
    
    msg = g_strdup_vprintf(format, va);
    gedit_utils_flash (msg);
    g_free(msg);
    
    va_end(va);
}

GtkWindow*    
seahorse_gedit_active_window (void)
{
    return GTK_WINDOW (gedit_get_active_window ());
}
