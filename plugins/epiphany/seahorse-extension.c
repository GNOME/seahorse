/*
 *  Copyright (C) 2006 Jean-François Rameau and Adam Schreiber
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "seahorse-extension.h"
#include "mozilla/mozilla-helper.h"

#include <epiphany/ephy-extension.h>
#include <epiphany/ephy-embed.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <gmodule.h>
#include <gtk/gtkaction.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkmenuitem.h>

#include <glib/gi18n-lib.h>

#include <string.h>

//#include <glib.h>
//#include <glib/gi18n.h>
#include <cryptui.h>
#include <dbus/dbus-glib.h>

#define SEAHORSE_EXTENSION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), TYPE_SEAHORSE_EXTENSION, SeahorseExtensionPrivate))

#define ENCRYPT_ACTION		"SeahorseExtEncrypt"
#define SIGN_ACTION         "SeahorseExtSign"
#define DECRYPT_ACTION      "SeahorseExtDecrypt"
#define IMPORT_ACTION       "SeahorseExtImport"
#define WINDOW_DATA_KEY		"SeahorseWindowData"

typedef enum {
    SEAHORSE_TEXT_TYPE_NONE,
    SEAHORSE_TEXT_TYPE_PLAIN,
    SEAHORSE_TEXT_TYPE_KEY,
    SEAHORSE_TEXT_TYPE_MESSAGE,
    SEAHORSE_TEXT_TYPE_SIGNED
} SeahorseTextType;

typedef struct _SeahorsePGPHeader {
    const gchar *header;
    const gchar *footer;
    SeahorseTextType type;
} SeahorsePGPHeader;    

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

struct SeahorseExtensionPrivate
{
	int dummy;
};

/* -----------------------------------------------------------------------------
 * Initialize Crypto 
 */
 
 /* Setup in init_crypt */
DBusGConnection *dbus_connection = NULL;
DBusGProxy      *dbus_key_proxy = NULL;
DBusGProxy      *dbus_crypto_proxy = NULL;
CryptUIKeyset   *dbus_keyset = NULL;

static gboolean
init_crypt ()
{
    GError *error = NULL;
    
    if (!dbus_connection) {
        dbus_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (!dbus_connection) {
            
            return FALSE;
        }

        dbus_key_proxy = dbus_g_proxy_new_for_name (dbus_connection, "org.gnome.seahorse",
                                               "/org/gnome/seahorse/keys",
                                               "org.gnome.seahorse.KeyService");
        
        dbus_crypto_proxy = dbus_g_proxy_new_for_name (dbus_connection, "org.gnome.seahorse",
                                               "/org/gnome/seahorse/crypto",
                                               "org.gnome.seahorse.CryptoService");
        
        dbus_keyset = cryptui_keyset_new ("openpgp");
    }
    
    return TRUE;
}

SeahorseTextType    
detect_text_type (const gchar *text, gint len, const gchar **start, const gchar **end)
{
    const SeahorsePGPHeader *header;
    const gchar *pos = NULL;
    const gchar *t;
    int i;
    
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
    
    return SEAHORSE_TEXT_TYPE_PLAIN;
}

typedef struct
{
	GtkUIManager *manager;
	GtkActionGroup *action_group;
	guint ui_id;
} WindowData;

static void seahorse_extension_class_init (SeahorseExtensionClass *klass);
static void seahorse_extension_iface_init (EphyExtensionIface *iface);
static void seahorse_extension_init       (SeahorseExtension *extension);

static GObjectClass *parent_class = NULL;

static GType type = 0;

GType
seahorse_extension_get_type (void)
{
	return type;
}

GType
seahorse_extension_register_type (GTypeModule *module)
{
	static const GTypeInfo our_info =
	{
		sizeof (SeahorseExtensionClass),
		NULL, /* base_init */
		NULL, /* base_finalize */
		(GClassInitFunc) seahorse_extension_class_init,
		NULL,
		NULL, /* class_data */
		sizeof (SeahorseExtension),
		0, /* n_preallocs */
		(GInstanceInitFunc) seahorse_extension_init
	};

	static const GInterfaceInfo extension_info =
	{
		(GInterfaceInitFunc) seahorse_extension_iface_init,
		NULL,
		NULL
	};

	type = g_type_module_register_type (module,
					    G_TYPE_OBJECT,
					    "SeahorseExtension",
					    &our_info, 0);

	g_type_module_add_interface (module,
				     type,
				     EPHY_TYPE_EXTENSION,
				     &extension_info);

	return type;
}

static void 
encrypt_seahorse_cb (GtkAction *action, EphyWindow *window)
{
	EphyEmbed *embed;
	const char *text;
    gchar **keys;
    gchar *signer = NULL;
    gchar *enctext = NULL;
    gboolean ret;
    
    init_crypt();
    
	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	/* ask gecko for the input */
	text = mozilla_get_text (embed);
	
	g_return_if_fail ((text != NULL) || (text[0] != '\0'));
    
    /* Get the recipient list */
    keys = cryptui_prompt_recipients (dbus_keyset, _("Choose Recipient Keys"), &signer);

    /* User may have cancelled */
    if (keys && *keys) {
        ret = dbus_g_proxy_call (dbus_crypto_proxy, "EncryptText", NULL, 
                                 G_TYPE_STRV, keys, 
                                 G_TYPE_STRING, signer, 
                                 G_TYPE_INT, 0,
                                 G_TYPE_STRING, text,
                                 G_TYPE_INVALID,
                                 G_TYPE_STRING, &enctext,
                                 G_TYPE_INVALID);
                                
    }
    
    g_strfreev(keys);
    g_free (signer);
    
    if (ret != TRUE) {
        g_free (enctext);
        return;
    }
	
	mozilla_set_text (embed, enctext);
}

static void 
sign_seahorse_cb (GtkAction *action, EphyWindow *window)
{
    EphyEmbed *embed;
	const char *text;
    gchar *signer = NULL;
    gchar *enctext = NULL;
    gboolean ret;
    
    init_crypt();
    
	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	/* ask gecko for the input */
	text = mozilla_get_text (embed);
	
	g_return_if_fail ((text != NULL) || (text[0] != '\0'));
    
    signer = cryptui_prompt_signer (dbus_keyset, _("Choose Key to Sign with"));
    if (signer == NULL)
        return;

    /* Perform the signing */
    ret = dbus_g_proxy_call (dbus_crypto_proxy, "SignText", NULL, 
                                G_TYPE_STRING, signer, 
                                G_TYPE_INT, 0,
                                G_TYPE_STRING, text,
                                G_TYPE_INVALID,
                                G_TYPE_STRING, &enctext,
                                G_TYPE_INVALID);
    g_free (signer);
    
    if (ret != TRUE) {
        g_free (enctext);
        return;
    }
	
	mozilla_set_text (embed, enctext);
}

/* When we try to 'decrypt' a key, this gets called */
static guint
import_keys (const gchar *text)
{
    gchar **keys, **k;
    gint nkeys = 0;
    gboolean ret;

    ret = dbus_g_proxy_call (dbus_key_proxy, "ImportKeys", NULL,
                             G_TYPE_STRING, "openpgp",
                             G_TYPE_STRING, text,
                             G_TYPE_INVALID,
                             G_TYPE_STRV, &keys,
                             G_TYPE_INVALID);
                             
    if (ret) {
        for (k = keys, nkeys = 0; *k; k++)
            nkeys++;
        g_strfreev (keys);
        
        if (!nkeys)
            dbus_g_proxy_call (dbus_key_proxy, "DisplayNotification", NULL,
                               G_TYPE_STRING, _("Import Failed"),
                               G_TYPE_STRING, _("Keys were found but not imported."),
                               G_TYPE_STRING, NULL,
                               G_TYPE_BOOLEAN,   FALSE,
                               G_TYPE_INVALID,
                               G_TYPE_INVALID);
    }
    
    return nkeys;
}

/* Decrypt an encrypted message */
static gchar*
decrypt_text (const gchar *text)
{
    gchar *rawtext = NULL;
    gchar *signer = NULL;
    gboolean ret;
    
    ret = dbus_g_proxy_call (dbus_crypto_proxy, "DecryptText", NULL,
                             G_TYPE_STRING, "openpgp",
                             G_TYPE_INT, 0,
                             G_TYPE_STRING, text,
                             G_TYPE_INVALID, 
                             G_TYPE_STRING, &rawtext,
                             G_TYPE_STRING, &signer,
                             G_TYPE_INVALID);

    if (ret) {
        g_free (signer);
        
        return rawtext;
    } else {
        dbus_g_proxy_call (dbus_key_proxy, "DisplayNotification", NULL,
                           G_TYPE_STRING, _("Decrypting Failed"),
                           G_TYPE_STRING, _("Text may be malformed."),
                           G_TYPE_STRING, NULL,
                           G_TYPE_BOOLEAN,   FALSE,
                           G_TYPE_INVALID,
                           G_TYPE_INVALID);
        return NULL;
    }
}

/* Verify a signed message */
static gchar*
verify_text (const gchar *text)
{
    gchar *rawtext = NULL;
    gchar *signer;
    gboolean ret;

    ret = dbus_g_proxy_call (dbus_crypto_proxy, "VerifyText", NULL,
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
        return NULL;
    }
}

static void 
dvi_seahorse_cb (GtkAction *action, EphyWindow *window)
{
    EphyEmbed *embed;
	const char *text;
    gchar *rawtext = NULL;
    SeahorseTextType type;
    gint keys = 0;
    
    init_crypt();
    
	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	/* ask gecko for the input */
	text = mozilla_get_text (embed);
	
	g_return_if_fail ((text != NULL) || (text[0] != '\0'));
    
    /* Try to figure out what it contains */
    type = detect_text_type (text, -1, NULL, NULL);
    
    if (type == SEAHORSE_TEXT_TYPE_NONE)
        return;
    
    switch (type) {

    /* A key, import it */
    case SEAHORSE_TEXT_TYPE_KEY:
        keys = import_keys (text);
        break;

    /* A message decrypt it */
    case SEAHORSE_TEXT_TYPE_MESSAGE:
        rawtext = decrypt_text (text);
        break;

    /* A message verify it */
    case SEAHORSE_TEXT_TYPE_SIGNED:
        rawtext = verify_text (text);
        break;

    default:
        g_assert_not_reached ();
        break;
    };
    
    /* We got replacement text */
    if (rawtext)
        mozilla_set_text (embed, rawtext);    
}
static gboolean
context_menu_cb (EphyEmbed *embed,
		 EphyEmbedEvent *event,
		 EphyWindow *window)
{
	gboolean is_input;
	GtkAction  *action;
	WindowData *data;
    const char *text;
    SeahorseTextType texttype;
    
	data = (WindowData *) g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_val_if_fail (data != NULL, FALSE);

    is_input = mozilla_is_input (embed);

    if (is_input == FALSE)
        return FALSE;
        
	text = mozilla_get_text (embed);
	if (text == NULL)
	    return FALSE;
	
    texttype = detect_text_type (text, -1, NULL, NULL);   

	action = gtk_action_group_get_action (data->action_group, ENCRYPT_ACTION);
	g_return_val_if_fail (action != NULL, FALSE);	

	gtk_action_set_sensitive (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_PLAIN)));
	gtk_action_set_visible (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_PLAIN)));

    action = gtk_action_group_get_action (data->action_group, SIGN_ACTION);
	g_return_val_if_fail (action != NULL, FALSE);	

	gtk_action_set_sensitive (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_PLAIN)));
	gtk_action_set_visible (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_PLAIN)));

    action = gtk_action_group_get_action (data->action_group, DECRYPT_ACTION);
	g_return_val_if_fail (action != NULL, FALSE);	

	gtk_action_set_sensitive (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_MESSAGE ||
            texttype == SEAHORSE_TEXT_TYPE_SIGNED)));
	gtk_action_set_visible (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_MESSAGE ||
            texttype == SEAHORSE_TEXT_TYPE_SIGNED)));

    action = gtk_action_group_get_action (data->action_group, IMPORT_ACTION);
	g_return_val_if_fail (action != NULL, FALSE);	

	gtk_action_set_sensitive (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_KEY)));
	gtk_action_set_visible (action, (is_input && (texttype == SEAHORSE_TEXT_TYPE_KEY)));
	
	return FALSE;
}

static void
build_ui (WindowData *data)
{
	GtkUIManager *manager = data->manager;
	guint ui_id;

	LOG ("Building UI");

	/* clean UI */
	if (data->ui_id != 0)
	{
		gtk_ui_manager_remove_ui (manager, data->ui_id);
		gtk_ui_manager_ensure_update (manager);
	}

	data->ui_id = ui_id = gtk_ui_manager_new_merge_id (manager);

	/* Add bookmarks to popup context (normal document) */
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyDocumentPopup",
			       "SeahorseExtSep0", NULL,
			       GTK_UI_MANAGER_SEPARATOR, FALSE);
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyDocumentPopup",
			       ENCRYPT_ACTION, ENCRYPT_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
    gtk_ui_manager_add_ui (manager, ui_id, "/EphyDocumentPopup",
			       SIGN_ACTION, SIGN_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
    gtk_ui_manager_add_ui (manager, ui_id, "/EphyDocumentPopup",
			       DECRYPT_ACTION, DECRYPT_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
    gtk_ui_manager_add_ui (manager, ui_id, "/EphyDocumentPopup",
			       IMPORT_ACTION, IMPORT_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);

	/* Add bookmarks to input popup context */
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyInputPopup",
			       "SeahorseExtSep0", NULL,
			       GTK_UI_MANAGER_SEPARATOR, FALSE);
	gtk_ui_manager_add_ui (manager, ui_id, "/EphyInputPopup",
			       ENCRYPT_ACTION, ENCRYPT_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
    gtk_ui_manager_add_ui (manager, ui_id, "/EphyInputPopup",
			       SIGN_ACTION, SIGN_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
    gtk_ui_manager_add_ui (manager, ui_id, "/EphyInputPopup",
			       DECRYPT_ACTION, DECRYPT_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
    gtk_ui_manager_add_ui (manager, ui_id, "/EphyInputPopup",
			       IMPORT_ACTION, IMPORT_ACTION,
			       GTK_UI_MANAGER_MENUITEM, FALSE);
			       
	gtk_ui_manager_ensure_update (manager);
}

static void
impl_attach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyTab *tab)
{
	EphyEmbed *embed;

	embed = ephy_tab_get_embed (tab);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	g_signal_connect (embed, "ge_context_menu",
			  G_CALLBACK (context_menu_cb), window);
}

static void
impl_detach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyTab *tab)
{
	EphyEmbed *embed;

	embed = ephy_tab_get_embed (tab);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	g_signal_handlers_disconnect_by_func
		(embed, G_CALLBACK (context_menu_cb), window);
}

static const GtkActionEntry action_entries [] =
{
	{ ENCRYPT_ACTION,
	  NULL,
	  N_("_Encrypt"),
	  NULL,
	  NULL,
	  G_CALLBACK (encrypt_seahorse_cb)
	},
	{ SIGN_ACTION,
	  NULL,
	  N_("_Sign"),
	  NULL,
	  NULL,
	  G_CALLBACK (sign_seahorse_cb)
	},
	{ DECRYPT_ACTION,
	  NULL,
	  N_("_Decrypt/Verify"),
	  NULL,
	  NULL,
	  G_CALLBACK (dvi_seahorse_cb)
	},
	{ IMPORT_ACTION,
	  NULL,
	  N_("_Import Key"),
	  NULL,
	  NULL,
	  G_CALLBACK (dvi_seahorse_cb)
	},
};

static void
impl_attach_window (EphyExtension *ext,
		    EphyWindow *window)
{
	GtkActionGroup *action_group;
	WindowData *data;

	LOG ("SeahorseExtension attach_window %p", window);

	/* Attach ui infos to the window */
	data = g_new0 (WindowData, 1);
	g_object_set_data_full (G_OBJECT (window), WINDOW_DATA_KEY, data,
				(GDestroyNotify) g_free);

	/* Create new action group for this extension */
	action_group = gtk_action_group_new ("SeahorseExtActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, action_entries,
				      G_N_ELEMENTS (action_entries), window);

	data->manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	data->action_group = action_group;

	gtk_ui_manager_insert_action_group (data->manager, action_group, -1);
	g_object_unref (action_group);

	/* now add the UI to the window */
	build_ui (data);
}

static void
impl_detach_window (EphyExtension *ext,
		    EphyWindow *window)
{
	WindowData *data;

	LOG ("SeahorseExtension detach_window");

	data = g_object_get_data (G_OBJECT (window), WINDOW_DATA_KEY);
	g_return_if_fail (data != NULL);

	gtk_ui_manager_remove_ui (data->manager, data->ui_id);
	gtk_ui_manager_ensure_update (data->manager);
	gtk_ui_manager_remove_action_group (data->manager, data->action_group);

	g_object_set_data (G_OBJECT (window), WINDOW_DATA_KEY, NULL);
}

static void
seahorse_extension_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
	iface->attach_tab = impl_attach_tab;
	iface->detach_tab = impl_detach_tab;
}

static void
seahorse_extension_init (SeahorseExtension *extension)
{
	LOG ("SeahorseExtension initialising");

	extension->priv = SEAHORSE_EXTENSION_GET_PRIVATE (extension);
}

static void
seahorse_extension_class_init (SeahorseExtensionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	g_type_class_add_private (object_class, sizeof (SeahorseExtensionPrivate));
}
