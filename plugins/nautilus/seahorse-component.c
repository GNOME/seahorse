
#include <gnome.h>
#include <libbonobo.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "seahorse-component.h"

static gchar*
get_path_from_url (const gchar *url)
{
	GnomeVFSURI *uri = NULL;
	gchar *escaped = NULL, *path = NULL;
	
	/* get uri from url */
	uri = gnome_vfs_uri_new (url);
	g_return_val_if_fail (uri != NULL, NULL);

	/* get escaped uri, unref old uri */
	escaped = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);
	gnome_vfs_uri_unref (uri);
	g_return_val_if_fail (escaped != NULL, NULL);
	
	/* get real path, free escaped uri */
	path = gnome_vfs_unescape_string (escaped, NULL);
	g_free (escaped);
	return path;
}

static void
impl_Bonobo_Listener_event (PortableServer_Servant servant, const CORBA_char *event_name,
			    const CORBA_any *args, CORBA_Environment *ev)
{
	SeahorseComponent *sc;
	const CORBA_sequence_CORBA_string *list;
	gchar *cmd, *path, *cmd_option;
	GString *str;
	//gint i;

	sc = SEAHORSE_COMPONENT (bonobo_object_from_servant (servant));

	/* not sure what this does */
	if (!CORBA_TypeCode_equivalent (args->_type, TC_CORBA_sequence_CORBA_string, ev))
		return;

	/* gets arg list ?? */
	list = (CORBA_sequence_CORBA_string *)args->_value;

	g_return_if_fail (sc != NULL);
	g_return_if_fail (list != NULL);

	/* get first path */
	path = get_path_from_url (list->_buffer[0]);
	/* start cmd string */
	str = g_string_new ("seahorse");

	/* get command line options */
	if (g_str_equal (event_name, "Import")) 
		cmd_option = g_strdup_printf ("--import=\"%s\"", path);
	else if (g_str_equal (event_name, "Encrypt")) 
		cmd_option = g_strdup_printf ("--encrypt=\"%s\"", path);
	else if (g_str_equal (event_name, "Sign")) 
		cmd_option = g_strdup_printf ("--sign=\"%s\"", path);
	else if (g_str_equal (event_name, "EncryptSign")) 
		cmd_option = g_strdup_printf ("--encrypt-sign=\"%s\"", path);
	else if (g_str_equal (event_name, "Decrypt")) 
		cmd_option = g_strdup_printf ("--decrypt=\"%s\"", path);
	else if (g_str_equal (event_name, "Verify")) 
		cmd_option = g_strdup_printf ("--verify=\"%s\"", path);
	else if (g_str_equal (event_name, "DecryptVerify")) 
		cmd_option = g_strdup_printf ("--decrypt-verify=\"%s\"", path);

	g_free (path);
       	g_string_append_printf (str, " %s ", cmd_option);

	/* appends any multiple files, not ready for this yet
	for (i = 0; i < list->_length; i++) {
		char *path = get_path_from_url (list->_buffer[i]);

		if (path == NULL) 
			continue;
		
		g_string_append_printf (str, " \"%s\"", path);
		g_free (path);
	}*/

	/* get actual command & run it */
	cmd = g_string_free (str, FALSE);
	g_spawn_command_line_async (cmd, NULL);

	g_free (cmd);
	g_free (cmd_option);
}

static void
seahorse_component_class_init (SeahorseComponentClass *class)
{
	POA_Bonobo_Listener__epv *epv = &class->epv;
	epv->event = impl_Bonobo_Listener_event;
}

static void
seahorse_component_init (SeahorseComponent *sc)
{}

BONOBO_TYPE_FUNC_FULL (SeahorseComponent, Bonobo_Listener,
		       BONOBO_TYPE_OBJECT, seahorse_component);
