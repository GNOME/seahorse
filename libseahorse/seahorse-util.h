/*
 * Seahorse
 *
 * Copyright (C) 2003 Jacob Perkins
 * Copyright (C) 2004-2005 Nate Nielsen
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
 
/**
 * A bunch of miscellaneous utility functions.
 */

#ifndef __SEAHORSE_UTIL_H__
#define __SEAHORSE_UTIL_H__

#include <gtk/gtk.h>
#include <time.h>
#include <gpgme.h>

#include "config.h"
#include "seahorse-context.h"

#ifdef WITH_SHARING
#include <avahi-client/client.h>
const AvahiPoll* seahorse_util_dns_sd_get_poll ();
#endif

typedef enum {
    SEAHORSE_CRYPT_SUFFIX,
    SEAHORSE_SIG_SUFFIX,
} SeahorseSuffix;

#define SEAHORSE_EXT_ASC ".asc"
#define SEAHORSE_EXT_SIG ".sig"
#define SEAHORSE_EXT_PGP ".pgp"
#define SEAHORSE_EXT_GPG ".gpg"

gchar*		seahorse_util_get_date_string		(const time_t		time);

#define SEAHORSE_GPGME_ERROR  (seahorse_util_gpgme_error_domain ())

GQuark      seahorse_util_gpgme_error_domain ();

#define SEAHORSE_ERROR  (seahorse_util_error_domain ())

GQuark      seahorse_util_error_domain ();

void        seahorse_util_gpgme_to_error        (gpgme_error_t gerr, 
                                                 GError** err);

void        seahorse_util_show_error            (GtkWindow          *parent,
                                                 const gchar        *heading,
                                                 const gchar        *message);
                                                 
void        seahorse_util_handle_gpgme          (gpgme_error_t      err,
                                                 const gchar*       desc, ...);

void        seahorse_util_handle_error          (GError*            err,
                                                 const char*        desc, ...);

gchar*      seahorse_util_write_data_to_text    (gpgme_data_t       data,
                                                 guint              *len);

guint       seahorse_util_read_data_block       (GString            *buf, 
                                                 gpgme_data_t       data, 
                                                 const gchar        *start, 
                                                 const gchar*       end);

gboolean    seahorse_util_print_fd          (int fd, 
                                             const char* data);

gboolean    seahorse_util_printf_fd         (int fd, 
                                             const char* data, ...);
                             
gchar*      seahorse_util_filename_for_keys (GList *keys);
                                             
gboolean    seahorse_util_uri_exists        (const gchar* uri);

gchar*      seahorse_util_uri_unique        (const gchar* uri);

gchar*      seahorse_util_uri_replace_ext   (const gchar *uri, 
                                             const gchar *ext);

const gchar* seahorse_util_uri_get_last     (const gchar* uri);

const gchar* seahorse_util_uri_split_last   (gchar* uri);

gchar**     seahorse_util_uris_expand       (const gchar **uris);

gboolean    seahorse_util_uris_package      (const gchar* package, 
                                             const gchar** uris);

GQuark      seahorse_util_detect_mime_type   (const gchar *mime);

GQuark      seahorse_util_detect_data_type   (const gchar *data,
                                              guint length);

GQuark      seahorse_util_detect_file_type   (const gchar *uri);

gboolean    seahorse_util_write_file_private            (const gchar* filename,
                                                         const gchar* contents,
                                                         GError **err);

GtkWidget*  seahorse_util_chooser_open_new              (const gchar *title,
                                                         GtkWindow *parent);

GtkWidget*  seahorse_util_chooser_save_new              (const gchar *title,
                                                         GtkWindow *parent);

void        seahorse_util_chooser_show_key_files        (GtkWidget *dialog);

void        seahorse_util_chooser_show_archive_files    (GtkWidget *dialog);

void        seahorse_util_chooser_set_filename          (GtkWidget *dialog, 
                                                         GList *keys);

gchar*      seahorse_util_chooser_open_prompt           (GtkWidget *dialog);

gchar*      seahorse_util_chooser_save_prompt           (GtkWidget *dialog);

gboolean	seahorse_util_check_suffix		(const gchar		*path,
                                             SeahorseSuffix     suffix);

gchar*		seahorse_util_add_suffix		(const gchar        *path,
                                             SeahorseSuffix     suffix,
                                             const gchar        *prompt);

gchar*      seahorse_util_remove_suffix     (const gchar        *path,
                                             const gchar        *prompt);

gchar**     seahorse_util_strvec_dup        (const gchar        **vec);

guint       seahorse_util_strvec_length       (const gchar      **vec);

gpgme_key_t* seahorse_util_keylist_to_keys    (GList *keys);

GList*       seahorse_util_keylist_sort       (GList *keys);

GList*       seahorse_util_keylist_splice     (GList *keys);

void         seahorse_util_free_keys          (gpgme_key_t* keys);

gboolean    seahorse_util_string_equals       (const gchar *s1, const gchar *s2);

gchar*      seahorse_util_string_up_first     (const gchar *orig);

void        seahorse_util_string_lower        (gchar *s);

GSList*     seahorse_util_string_slist_free   (GSList *slist);

GSList*     seahorse_util_string_slist_copy   (GSList *slist);

gboolean    seahorse_util_string_slist_equal  (GSList *sl1, GSList *sl2);

void        seahorse_util_determine_popup_menu_position  (GtkMenu *menu,
                                                           int *x,
                                                           int *y,
                                                           gboolean *push_in,
                                                           gpointer gdata);

#define     seahorse_util_wait_until(expr)          \
    while (!(expr)) {                               \
        g_thread_yield ();                          \
        g_main_context_iteration (NULL, FALSE);     \
    }

#ifdef _DEBUG
#define DBG_PRINT(x) g_printerr x
#else
#define DBG_PRINT(x)
#endif

#endif /* __SEAHORSE_UTIL_H__ */
