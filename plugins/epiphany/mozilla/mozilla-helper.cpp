/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
 *  Copyright (C) 2004 Jean-François Rameau
 *  Copyright (C) 2006 Adam Schreiber
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

#include "mozilla-config.h"
#include "config.h"

#include <glib.h>

#include <nsStringAPI.h>

#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#include <nsCOMPtr.h>
#include <nsIDOMElement.h>
#include <nsIDOMHTMLInputElement.h>
#include <nsIDOMHTMLTextAreaElement.h>
#include <nsIDOMNSHTMLInputElement.h>
#include <nsIDOMNSHTMLTextAreaElement.h>
#include <nsIDOMWindow.h>
#include <nsISelection.h>
#include <nsIWebBrowserFocus.h>
#include <nsIWebBrowser.h>
#include <nsMemory.h>

#include "mozilla-helper.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <cryptui.h>
#include <dbus/dbus-glib.h>

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

template <class T>
void set_value (nsIDOMElement *aElement, const char *value)
{
	// Get full text in element
	nsString text; 
	nsCOMPtr<T> element (do_QueryInterface(aElement));

	nsString nsValue;
	NS_CStringToUTF16 (nsCString(value),
			   NS_CSTRING_ENCODING_UTF8, nsValue);

	element->SetValue (nsValue);
}

template <class T>
char * get_value (nsIDOMElement *aElement)
{
	// Get full text in element
	nsString text; 
	nsCOMPtr<T> element (do_QueryInterface(aElement));
	NS_ENSURE_TRUE (element, NULL);

	element->GetValue(text);
	if (text.IsEmpty ()) return NULL;

	nsCString cText; 
	NS_UTF16ToCString(text, NS_CSTRING_ENCODING_UTF8, cText);

	return g_strdup(cText.get());
}

extern "C" char*
seahorse_encrypt (char *text)
{
    gchar **keys;
    gchar *signer = NULL;
    gchar *enctext = NULL;
    gboolean ret;
    
    init_crypt();
    
    g_return_val_if_fail ((text != NULL) || (text[0] != '\0'), NULL);
    
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
                                
        g_strfreev(keys);
        g_free (signer);
    }
    
    if (ret != TRUE) {
        g_free (enctext);
        return NULL;
    }
    
    return enctext;       
}

extern "C" void
mozilla_encrypt (EphyEmbed *embed)
{
	nsCOMPtr<nsIWebBrowser> browser;
	gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (embed),
			getter_AddRefs (browser));
	nsCOMPtr<nsIWebBrowserFocus> focus (do_QueryInterface(browser));
	if (!focus) return;

	nsCOMPtr<nsIDOMElement> domElement;
	focus->GetFocusedElement (getter_AddRefs(domElement));
	if (!domElement) return;

	char *value;
	// Try with a textarea
	value = get_value <nsIDOMHTMLTextAreaElement> (domElement);
	if (value)
	{
		char *encrypted_value;
	
		encrypted_value = seahorse_encrypt (value);

		set_value <nsIDOMHTMLTextAreaElement> (domElement, encrypted_value);

        g_free (encrypted_value);
        
		return;
	}

	// Then with any input
	// Take care of password fields
	nsString text;
	nsCOMPtr<nsIDOMHTMLInputElement> input (do_QueryInterface(domElement));
	input->GetType (text);
	const PRUnichar *str = text.get ();
	if (!(str[0] == 't' && str[1] == 'e' && str[2] == 'x' && str[3] == 't' && str[4] == '\0')) return;

	value = get_value <nsIDOMHTMLInputElement> (domElement);
	if (value)
	{
		char *encrypted_value;
	
		encrypted_value = seahorse_encrypt (value);

		set_value <nsIDOMHTMLInputElement> (domElement, encrypted_value);
		
        g_free (encrypted_value);

		return;
	}
}

extern "C" gboolean
mozilla_is_input (EphyEmbed *embed)
{
	nsCOMPtr<nsIWebBrowser> browser;
	gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (embed),
			getter_AddRefs (browser));
	nsCOMPtr<nsIWebBrowserFocus> focus (do_QueryInterface(browser));
	if (!focus) return FALSE;

	nsCOMPtr<nsIDOMElement> domElement;
	focus->GetFocusedElement (getter_AddRefs(domElement));
	if (!domElement) return FALSE;

	nsCOMPtr<nsIDOMHTMLTextAreaElement> nodeAsArea (do_QueryInterface(domElement));
	if (nodeAsArea) return TRUE;

	nsCOMPtr<nsIDOMHTMLInputElement> nodeAsInput (do_QueryInterface(domElement));
	if (nodeAsInput) return TRUE;

	return FALSE;
}
