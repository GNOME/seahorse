/* 
 * Seahorse
 * 
 * Copyright (C) 2008 Stefan Walter
 * 
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *  
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include "config.h"

#include "crui-x509-cert.h"
#include "crui-x509-cert-dialog.h"
#include "crui-x509-cert-simple.h"

#include <gtk/gtk.h>

#include <glib/gstdio.h>

static void
show_certificate_dialog (void)
{
	CruiX509CertSimple *simple;
	CruiX509CertDialog *dialog;
	GError *err = NULL;
	
	simple = crui_x509_cert_simple_new_from_file ("files/test-certificate-1.der", &err);
	if (!simple) {
		g_warning ("couldn't load certificate: %s", err->message);
		return;
	}
		
	dialog = crui_x509_cert_dialog_new (CRUI_X509_CERT (simple));
	g_object_ref (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	
	g_object_unref (dialog);
	g_object_unref (simple);
}

static void 
chdir_base_dir (char* argv0)
{
	gchar *dir, *base;

	dir = g_path_get_dirname (argv0);
	if (g_chdir (dir) < 0)
		g_warning ("couldn't change directory to: %s", dir);
	
	base = g_path_get_basename (dir);
	if (strcmp (base, ".libs") == 0) {
		if (g_chdir ("..") < 0)
			g_warning ("couldn't change directory to ..");
	}

	g_free (base);
	g_free (dir);
}

int
main (int argc, char **argv)
{
    const gchar *arg = "certificate";
    
    chdir_base_dir (argv[0]);
    gtk_init(&argc, &argv);
    
    if (argc > 1)
	    arg = argv[1];
	
    if (g_ascii_strcasecmp (arg, "certificate") == 0) 
	    show_certificate_dialog ();
    else
	    g_warning ("must specify something valid to display");
	    
    return 0;
}