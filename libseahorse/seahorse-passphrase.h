/*
 * Seahorse
 *
 * Copyright (C) 2006 Stefan Walter
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
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
