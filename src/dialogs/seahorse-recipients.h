/*
 * Seahorse
 *
 * Copyright (C) 2002 Jacob Perkins
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

#ifndef __SEAHORSE_RECIPIENTS_H__
#define __SEAHORSE_RECIPIENTS_H__

/* SeahorseRecipients is a SeahorseWidget for constructing a GpgmeRecipients list.
 * It contains a recipient list, two key stores,
 * and can prompt user if key's must be at least Fully Valid. */

#include <glib.h>
#include <gpgme.h>

#include "seahorse-context.h"
#include "seahorse-widget.h"
#include "seahorse-key-store.h"

#define SEAHORSE_TYPE_RECIPIENTS		(seahorse_recipients_get_type ())
#define SEAHORSE_RECIPIENTS(obj)		(GTK_CHECK_CAST ((obj), SEAHORSE_TYPE_RECIPIENTS, SeahorseRecipients))
#define SEAHORSE_RECIPIENTS_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), SEAHORSE_TYPE_RECIPIENTS, SeahorseRecipientsClass))
#define SEAHORSE_IS_RECIPIENTS(obj)		(GTK_CHECK_TYPE ((obj), SEAHORSE_TYPE_RECIPIENTS))
#define SEAHORSE_IS_RECIPIENTS_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), SEAHORSE_TYPE_RECIPIENTS))
#define SEAHORSE_RECIPIENTS_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS ((obj), SEAHORSE_TYPE_RECIPIENTS, SeahorseRecipientsClass))

typedef struct _SeahorseRecipients SeahorseRecipients;
typedef struct _SeahorseRecipientsClass SeahorseRecipientsClass;

struct _SeahorseRecipients
{
	SeahorseWidget		parent;
	
	/*< public >*/
	GpgmeRecipients		recips;

	/*< private >*/
	gboolean		need_validity;
	SeahorseKeyStore	*all_keys;
	SeahorseKeyStore	*recipient_keys;
};

struct _SeahorseRecipientsClass
{
	SeahorseWidgetClass	parent_class;
};

SeahorseWidget*	seahorse_export_recipients_new (SeahorseContext		*sctx);

SeahorseWidget*	seahorse_encrypt_recipients_new	(SeahorseContext	*sctx);

GpgmeRecipients	seahorse_recipients_run	(SeahorseRecipients	*srecips);

#endif /* __SEAHORSE_RECIPIENTS_H__ */
