
#include <gnome.h>
#include <bonobo.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnomevfs/gnome-vfs-utils.h>

/* this is the listener callback when creating a new listener
 * it is called when any registered events are activated
 */
static void
listener_callback (BonoboListener *listener, const gchar *event_name,
		   const CORBA_any *any, CORBA_Environment *ev, gpointer data)
{
	const CORBA_sequence_CORBA_string *list;
    const gchar *cmd_option;
	gchar *cmd, *t;
	GString *str;
	gint i;

	/* not sure what this does */
	if (!CORBA_TypeCode_equivalent (any->_type, TC_CORBA_sequence_CORBA_string, ev))
		return;

	/* gets arg list ?? */
	list = (CORBA_sequence_CORBA_string *)any->_value;

	g_return_if_fail (list != NULL);

	/* start cmd string */
	str = g_string_new ("seahorse");

	/* get command line options */
	if (g_str_equal (event_name, "Import")) 
		cmd_option = "--import";
	else if (g_str_equal (event_name, "Encrypt")) 
		cmd_option = "--encrypt";
	else if (g_str_equal (event_name, "Sign")) 
		cmd_option = "--sign";
	else if (g_str_equal (event_name, "EncryptSign")) 
		cmd_option = "--encrypt";
	else if (g_str_equal (event_name, "Decrypt")) 
		cmd_option = "--decrypt";
	else if (g_str_equal (event_name, "Verify")) 
		cmd_option = "--verify";

   	g_string_append_printf (str, " %s ", cmd_option);

	/* appends any multiple files */
	for (i = 0; i < list->_length; i++) {
        if (list->_buffer[i] == NULL)
            continue;
            
		t = g_shell_quote (list->_buffer[i]);
		g_string_append_printf (str, " %s", t);
		g_free (t);
	}

	/* get actual command & run it */
	cmd = g_string_free (str, FALSE);
	g_spawn_command_line_async (cmd, NULL);

	g_free (cmd);
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
