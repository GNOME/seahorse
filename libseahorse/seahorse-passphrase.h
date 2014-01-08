/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#ifndef __SEAHORSE_PASSPHRASE__
#define __SEAHORSE_PASSPHRASE__

#include <gtk/gtk.h>

#define SEAHORSE_PASS_BAD    0x00000001
#define SEAHORSE_PASS_NEW    0x01000000

GtkDialog*      seahorse_passphrase_prompt_show     (const gchar *title,
                                                     const gchar *description,
                                                     const gchar *prompt,
                                                     const gchar *check,
                                                     gboolean confirm);

const gchar*    seahorse_passphrase_prompt_get      (GtkDialog *dialog);

gboolean        seahorse_passphrase_prompt_checked  (GtkDialog *dialog);

#endif /* __SEAHORSE_PASSPHRASE__ */
