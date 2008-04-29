
#include "config.h"

#include "seahorse-pgp-dialogs.h"

#include "seahorse-util.h"

#include <gpgme.h>

void
seahorse_pgp_handle_gpgme_error (gpgme_error_t err, const char* desc, ...)
{
	gchar *t = NULL;

	switch (gpgme_err_code(err)) {
	case GPG_ERR_CANCELED:
	case GPG_ERR_NO_ERROR:
	case GPG_ERR_ECANCELED:
		return;
	default: 
		break;
	}

	va_list ap;
	va_start(ap, desc);
  
	if (desc) 
		t = g_strdup_vprintf (desc, ap);

	va_end(ap);
        
	seahorse_util_show_error (NULL, t, gpgme_strerror (err));
	g_free(t);
}
