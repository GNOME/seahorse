/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
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

#ifndef __SEAHORSE_UTIL_H__
#define __SEAHORSE_UTIL_H__

#include <gtk/gtk.h>
#include <time.h>
#include <gpgme.h>

#include "seahorse-context.h"

typedef enum {
	SEAHORSE_CRYPT_SUFFIX,
	SEAHORSE_SIG_SUFFIX,
	SEAHORSE_ASC_SUFFIX
} SeahorseSuffix;

typedef enum {
    SEAHORSE_TEXT_TYPE_NONE,
    SEAHORSE_TEXT_TYPE_KEY,
    SEAHORSE_TEXT_TYPE_MESSAGE,
    SEAHORSE_TEXT_TYPE_SIGNED
} SeahorseTextType;

gchar*		seahorse_util_get_date_string		(const time_t		time);

#define SEAHORSE_GPGME_ERROR  (seahorse_util_gpgme_error_domain())
GQuark      seahorse_util_gpgme_error_domain();

void        seahorse_util_gpgme_to_error        (gpgme_error_t gerr, 
                                                 GError** err);

void        seahorse_util_show_error            (GtkWindow          *parent,
                                                 const char*        msg);
                                                 
void        seahorse_util_handle_error          (gpgme_error_t      err,
                                                 const gchar*       desc, ...);

void        seahorse_util_handle_gerror         (GError*            err,
                                                 const char*        desc, ...);
                                                 
gpgme_error_t	seahorse_util_write_data_to_file	(const gchar		*path,
							 gpgme_data_t		data);

gchar*		seahorse_util_write_data_to_text	(gpgme_data_t		data);

gboolean    seahorse_util_print_fd          (int fd, 
                                             const char* data);

gboolean    seahorse_util_printf_fd         (int fd, 
                                             const char* data, ...);

SeahorseTextType  seahorse_util_detect_text (const gchar *text, 
                                             gint len, 
                                             const gchar **start, 
                                             const gchar **end);
                                                                                              
gboolean    seahorse_util_uri_exists        (const gchar* uri);

gchar*      seahorse_util_uri_unique        (const gchar* uri);

gchar*      seahorse_util_uri_replace_ext   (const gchar *uri, 
                                             const gchar *ext);

const gchar* seahorse_util_uri_get_last     (const gchar* uri);

const gchar* seahorse_util_uri_split_last   (gchar* uri);

gchar**     seahorse_util_uris_expand       (const gchar **uris);

gboolean    seahorse_util_uris_package      (const gchar* package, 
                                             const gchar** uris);

gchar*      seahorse_util_uri_choose_save   (GtkFileChooserDialog *chooser);

gboolean	seahorse_util_check_suffix		(const gchar		*path,
                                             SeahorseSuffix     suffix);

gchar*		seahorse_util_add_suffix		(gpgme_ctx_t		ctx,
                                             const gchar        *path,
                                             SeahorseSuffix     suffix,
                                             const gchar        *prompt);

gchar*      seahorse_util_remove_suffix     (const gchar        *path,
                                             const gchar        *prompt);

gchar**     seahorse_util_strvec_dup        (const gchar        **vec);

gpgme_key_t* seahorse_util_keylist_to_keys    (GList *keys);

GList*       seahorse_util_keylist_sort       (GList *keys);

GList*       seahorse_util_keylist_splice     (GList *keys);

void         seahorse_util_free_keys          (gpgme_key_t* keys);

GSList*     seahorse_util_string_slist_free   (GSList *slist);

GSList*     seahorse_util_string_slist_copy   (GSList *slist);

#define     seahorse_util_wait_until(expr)          \
    while (!(expr)) {                               \
        g_thread_yield ();                          \
        g_main_context_iteration (NULL, FALSE);     \
    }                                               

#endif /* __SEAHORSE_UTIL_H__ */
