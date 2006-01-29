/*
 * Seahorse
 *
 * Copyright (C) 2005 Nate Nielsen
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
 
#include <glib.h>
#include <glib-object.h>
#include <gedit/gedit-app.h>
#include <gedit/gedit-plugin.h>
#include <gedit/gedit-debug.h>
#include <gedit/gedit-statusbar.h>

#include "seahorse-context.h"
#include "seahorse-gedit.h"

/* -----------------------------------------------------------------------------
 * OBJECT DECLARATIONS
 */

#define SEAHORSE_TYPE_GEDIT_PLUGIN			(seahorse_gedit_plugin_get_type ())
#define SEAHORSE_GEDIT_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), SEAHORSE_TYPE_GEDIT_PLUGIN, SeahorseGeditPlugin))
#define SEAHORSE_GEDIT_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), SEAHORSE_TYPE_GEDIT_PLUGIN, SeahorseGeditPluginClass))
#define SEAHORSE_IS_GEDIT_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_TYPE ((o), SEAHORSE_TYPE_GEDIT_PLUGIN))
#define SEAHORSE_IS_GEDIT_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), SEAHORSE_TYPE_GEDIT_PLUGIN))
#define SEAHORSE_GEDIT_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), SEAHORSE_TYPE_GEDIT_PLUGIN, SeahorseGeditPluginClass))

typedef struct _SeahorseGeditPlugin			SeahorseGeditPlugin;
typedef struct _SeahorseGeditPluginClass	SeahorseGeditPluginClass;

struct _SeahorseGeditPlugin {
	GeditPlugin parent;
	
	/* <public> */
	SeahorseContext *sctx;
};

struct _SeahorseGeditPluginClass {
	GeditPluginClass parent_class;
};

/* -----------------------------------------------------------------------------
 * PLUGIN DECLARATIONS
 */

#define WINDOW_DATA_KEY "seahorse-gedit-plugin-window-data"
#define MENU_PATH "/MenuBar/EditMenu/EditOps_5"

#define MENU_ITEM_SIGN "Sign"
#define MENU_ITEM_DECRYPT "Decrypt"
#define MENU_ITEM_ENCRYPT "Encrypt"

typedef struct {
	GtkActionGroup *action_group;
	guint ui_id;
} WindowData;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_gedit_plugin (GTypeModule *module);
GEDIT_PLUGIN_REGISTER_TYPE(SeahorseGeditPlugin, seahorse_gedit_plugin);

/* -----------------------------------------------------------------------------
 * INTERNAL
 */

static void
encrypt_cb (GtkAction *action, SeahorseContext *sctx)
{
    GeditWindow *win;
	GeditDocument *doc;

    win = seahorse_gedit_active_window ();
    g_return_if_fail (win);

    doc = gedit_window_get_active_document(win);
	if(doc)
		seahorse_gedit_encrypt (sctx, doc);
}

static void
decrypt_cb (GtkAction *action, SeahorseContext *sctx)
{
    GeditWindow *win;
	GeditDocument *doc;

    win = seahorse_gedit_active_window ();
    g_return_if_fail (win);

    doc = gedit_window_get_active_document(win);
	if(doc)
		seahorse_gedit_decrypt (sctx, doc);
}

static void
sign_cb (GtkAction *action, SeahorseContext *sctx)
{
    GeditWindow *win;
	GeditDocument *doc;

    win = seahorse_gedit_active_window ();
    g_return_if_fail (win);

    doc = gedit_window_get_active_document(win);
	if(doc)
		seahorse_gedit_sign (sctx, doc);
}

static void
free_window_data (WindowData *data)
{
    if(data->action_group)
        g_object_unref (data->action_group);
    data->action_group = NULL;
    
    g_free (data);
}

static const GtkActionEntry action_entries[] =
{
	{ MENU_ITEM_ENCRYPT, NULL, N_("_Encrypt..."), NULL,
	  	N_("Encrypt the selected text"), G_CALLBACK (encrypt_cb) },
	{ MENU_ITEM_DECRYPT, NULL, N_("Decr_ypt/Verify"), NULL,
		N_("Decrypt and/or Verify text"), G_CALLBACK (decrypt_cb) },
	{ MENU_ITEM_SIGN, NULL, N_("Sig_n..."), NULL,
		N_("Sign the selected text"), G_CALLBACK (sign_cb) },
};

/* -----------------------------------------------------------------------------
 * OBJECT
 */

static void
seahorse_gedit_plugin_init (SeahorseGeditPlugin *splugin)
{
	splugin->sctx = seahorse_context_new ();
    seahorse_context_load_keys (splugin->sctx, FALSE);
	SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "seahorse gedit plugin inited");	
}

static void
seahorse_gedit_plugin_update_ui (GeditPlugin *plugin, GeditWindow *window)
{
	WindowData *data;
    GeditDocument *doc;
    gboolean sensitive;

	data = (WindowData*)g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_if_fail (data != NULL);

    doc = gedit_window_get_active_document(window);
	
    sensitive = (doc && gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)) > 0);
	gtk_action_group_set_sensitive (data->action_group, sensitive);	
}

static void
seahorse_gedit_plugin_activate (GeditPlugin *plugin, GeditWindow *window)
{
	SeahorseGeditPlugin *splugin = SEAHORSE_GEDIT_PLUGIN (plugin);
	GtkUIManager *manager;
    WindowData *data;
	
	manager = gedit_window_get_ui_manager (window);
	g_return_if_fail (manager != NULL);
	
	data = g_new0 (WindowData, 1);
	
	data->action_group = gtk_action_group_new ("SeahorseGeditPluginActions");
	gtk_action_group_set_translation_domain (data->action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (data->action_group, action_entries,
				      		      G_N_ELEMENTS (action_entries), splugin->sctx);
	gtk_ui_manager_insert_action_group (manager, data->action_group, -1);

	data->ui_id = gtk_ui_manager_new_merge_id (manager);

	g_object_set_data_full (G_OBJECT (window), WINDOW_DATA_KEY,  data, 
							(GDestroyNotify)free_window_data);

	gtk_ui_manager_add_ui (manager, data->ui_id, MENU_PATH, MENU_ITEM_SIGN, 
			       		   MENU_ITEM_SIGN, GTK_UI_MANAGER_MENUITEM,  FALSE);
	gtk_ui_manager_add_ui (manager, data->ui_id, MENU_PATH, MENU_ITEM_DECRYPT, 
			       		   MENU_ITEM_DECRYPT, GTK_UI_MANAGER_MENUITEM,  FALSE);
	gtk_ui_manager_add_ui (manager, data->ui_id, MENU_PATH, MENU_ITEM_ENCRYPT, 
			       		   MENU_ITEM_ENCRYPT, GTK_UI_MANAGER_MENUITEM,  FALSE);

	seahorse_gedit_plugin_update_ui (plugin, window);
}

static void
seahorse_gedit_plugin_deactivate (GeditPlugin *plugin, GeditWindow *window)
{
	GtkUIManager *manager;
	WindowData *data;

	manager = gedit_window_get_ui_manager (window);
	g_return_if_fail (manager != NULL);

	data = (WindowData*)g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_if_fail (data != NULL);

	gtk_ui_manager_remove_ui (manager, data->ui_id);
	gtk_ui_manager_remove_action_group (manager, data->action_group);

	g_object_set_data (G_OBJECT (window), WINDOW_DATA_KEY, NULL);
}

static void
seahorse_gedit_plugin_finalize (GObject *object)
{
	SeahorseGeditPlugin *splugin = SEAHORSE_GEDIT_PLUGIN (object);
	
	if (splugin->sctx)
		seahorse_context_destroy (splugin->sctx);
	splugin->sctx = NULL;
	
	G_OBJECT_CLASS (seahorse_gedit_plugin_parent_class)->finalize (object);

	SEAHORSE_GEDIT_DEBUG (DEBUG_PLUGINS, "seahorse gedit plugin destroyed");	
}

static void
seahorse_gedit_plugin_class_init (SeahorseGeditPluginClass *klass)
{
	GObjectClass *gclass = G_OBJECT_CLASS (klass);
	GeditPluginClass *plugin_class = GEDIT_PLUGIN_CLASS (klass);
	seahorse_gedit_plugin_parent_class = g_type_class_peek_parent (klass);
	
	gclass->dispose = seahorse_gedit_plugin_finalize;
	
	plugin_class->activate = seahorse_gedit_plugin_activate;
	plugin_class->deactivate = seahorse_gedit_plugin_deactivate;
	plugin_class->update_ui = seahorse_gedit_plugin_update_ui;
	
}

/* -----------------------------------------------------------------------------
 * UTILITIES
 */

void        
seahorse_gedit_flash (const gchar *format, ...)
{
    GeditWindow *win;
    GeditStatusbar *status;
    va_list va;
    gchar *msg; 

    win = seahorse_gedit_active_window ();
    g_return_if_fail (win);
    
    status = GEDIT_STATUSBAR (gedit_window_get_statusbar (win));
    g_return_if_fail (status);
    
    va_start(va, format);
    
    msg = g_strdup_vprintf(format, va);
    gedit_statusbar_flash_message (status, 0, "%s", msg);
    g_free(msg);
    
    va_end(va);
}

GeditWindow*    
seahorse_gedit_active_window (void)
{
    GeditApp *app;
    
    app = gedit_app_get_default ();
    g_return_val_if_fail (app, NULL);
    
    return gedit_app_get_active_window (app);
}
