
#include <gnome.h>
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>

/* helper method borrowed from file-roller */
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

/* this is the listener callback when creating a new listener
 * it is called when any registered events are activated
 */
static void
listener_callback (BonoboListener *listener, const gchar *event_name,
		   const CORBA_any *any, CORBA_Environment *ev, gpointer data)
{
	const CORBA_sequence_CORBA_string *list;
	gchar *cmd, *path, *cmd_option;
	GString *str;
	//gint i;

	/* not sure what this does */
	if (!CORBA_TypeCode_equivalent (any->_type, TC_CORBA_sequence_CORBA_string, ev))
		return;

	/* gets arg list ?? */
	list = (CORBA_sequence_CORBA_string *)any->_value;

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

/* this is the shared object factory method
 * it is called when a registered server with the IID has an event
 */
static CORBA_Object
shlib_make_object (PortableServer_POA poa, const gchar *iid,
		   gpointer impl_ptr, CORBA_Environment *ev)
{
	BonoboListener *listener;
	
	listener = bonobo_listener_new (listener_callback, NULL);
	bonobo_activation_plugin_use (poa, impl_ptr);
	return CORBA_Object_duplicate (BONOBO_OBJREF (listener), ev);
}

/* this is the plugin list with the IID and activate callback */
static const BonoboActivationPluginObject plugin_list[] = {
	{ "OAFIID:Seahorse_PGP_File_All",	shlib_make_object },
	{ "OAFIID:Seahorse_PGP_File_Encrypted",	shlib_make_object },
	{ "OAFIID:Seahorse_PGP_File_Signed",	shlib_make_object },
	{ "OAFIID:Seahorse_PGP_File_Keys",	shlib_make_object },
	{ NULL }
};

/* this registers the plugin list with bonobo */
const BonoboActivationPlugin Bonobo_Plugin_info = {
	plugin_list, "Seahorse PGP File Listener"
};
