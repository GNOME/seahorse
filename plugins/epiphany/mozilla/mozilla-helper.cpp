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

extern "C" const char*
mozilla_get_text (EphyEmbed *embed)
{
    nsCOMPtr<nsIWebBrowser> browser;
	gtk_moz_embed_get_nsIWebBrowser (GTK_MOZ_EMBED (embed),
			getter_AddRefs (browser));
	nsCOMPtr<nsIWebBrowserFocus> focus (do_QueryInterface(browser));
	if (!focus) 
	    return NULL;

	nsCOMPtr<nsIDOMElement> domElement;
	focus->GetFocusedElement (getter_AddRefs(domElement));
	if (!domElement) 
	    return NULL;

	char *value;
	// Try with a textarea
	value = get_value <nsIDOMHTMLTextAreaElement> (domElement);
	if (value)   
		return value;

	// Then with any input
	// Take care of password fields
	nsString text;
	nsCOMPtr<nsIDOMHTMLInputElement> input (do_QueryInterface(domElement));
	if (text.Length () > 0) {
    	input->GetType (text);
    	const PRUnichar *str = text.get ();
    	if (!(str[0] == 't' && str[1] == 'e' && str[2] == 'x' && str[3] == 't' && str[4] == '\0')) 
    	    return NULL;

    	value = get_value <nsIDOMHTMLInputElement> (domElement);
    	if (value)
            return value;
    }
    
    return NULL;
}

extern "C" void
mozilla_set_text (EphyEmbed *embed, char *new_text)
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
		set_value <nsIDOMHTMLTextAreaElement> (domElement, new_text);

        g_free (new_text);
        
		return;
	}

	// Then with any input
	// Take care of password fields
	nsString text;
	nsCOMPtr<nsIDOMHTMLInputElement> input (do_QueryInterface(domElement));
	input->GetType (text);
	const PRUnichar *str = text.get ();
	if (!(str[0] == 't' && str[1] == 'e' && str[2] == 'x' && str[3] == 't' && str[4] == '\0'))  
	    return;

	value = get_value <nsIDOMHTMLInputElement> (domElement);
	if (value)
	{
		set_value <nsIDOMHTMLInputElement> (domElement, new_text);
		
        g_free (new_text);

		return;
	}
}
