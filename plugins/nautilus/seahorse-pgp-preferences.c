
#include <gnome.h>
#include <config.h>
#include <libbonoboui.h>

capplet_response (GtkDialog *dialog, gint response, gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
	gtk_exit (0);
}

int
main (int argc, char **argv)
{
	GtkWidget *widget, *control;

#ifdef ENABLE_NLS
        bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);          
#endif

        gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
                GNOME_PARAM_HUMAN_READABLE_NAME, _("PGP Preferences"),
                GNOME_PARAM_APP_DATADIR, DATA_DIR, NULL);


	widget = gtk_dialog_new_with_buttons ("PGP Preferences", NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	g_signal_connect_after (GTK_DIALOG (widget), "response",
		G_CALLBACK (capplet_response), NULL);

	control = bonobo_widget_new_control ("OAFIID:Seahorse_PGP_Controls", NULL);
	
	bonobo_widget_set_property (BONOBO_WIDGET (control), "ascii_armor",
		BONOBO_ARG_BOOLEAN, CORBA_TRUE, NULL);
	bonobo_widget_set_property (BONOBO_WIDGET (control), "text_mode",
		BONOBO_ARG_BOOLEAN, CORBA_TRUE, NULL);
	bonobo_widget_set_property (BONOBO_WIDGET (control), "encrypt_self",
		BONOBO_ARG_BOOLEAN, CORBA_TRUE, NULL);
	bonobo_widget_set_property (BONOBO_WIDGET (control), "default_key",
		BONOBO_ARG_BOOLEAN, CORBA_TRUE, NULL);
	
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (widget)->vbox), control);

	gtk_widget_show_all (widget);
	gtk_main();
	return 0;
}
