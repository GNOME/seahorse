/*
 * Seahorse
 *
 * Copyright (C) 2003 Nate Nielsen
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

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <gpgme.h>
#include <string.h>
#include <errno.h>

#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-gpg-options.h"

#define  GPG_CONF_HEADER    "# FILE AUTO CREATED BY SEAHORSE\n\n"
#define  GPG_VERSION_PREFIX   "1.2."

static gchar gpg_homedir[MAXPATHLEN];
static gboolean gpg_options_inited = FALSE;

/* Finds and opens a relevant configuration file, creates if not found */
static GIOChannel *
open_config_file (gboolean read, GError **err)
{
    GIOChannel *ret = NULL;
    gchar *conf = NULL;
    gchar *opts = NULL;
    gboolean created = FALSE;

    g_assert (gpg_options_inited);

    /* Check for and open ~/.gnupg/gpg.conf */
    conf = g_strconcat (gpg_homedir, "/gpg.conf", NULL);
    if (g_file_test (conf, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) {
        ret = g_io_channel_new_file (conf, read ? "r" : "r+", err);
    } else {
        /* Check for and open ~/.gnupg/options */
        opts = g_strconcat (gpg_homedir, "/options");
        if (g_file_test (opts, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) {
            ret = g_io_channel_new_file (opts, read ? "r" : "r+", err);
        }

        /* Neither of the above exists, so create ~/.gnupg/gpg.conf */
        else {
            ret = g_io_channel_new_file (conf, "w+", err);
            created = TRUE;
        }
    }

    g_free (conf);
    g_free (opts);

    if (ret) {
        /* We don't want more than one writing at once */
        if (flock (g_io_channel_unix_get_fd (ret), read ? LOCK_SH : LOCK_EX) == -1) {
            g_set_error (err, G_IO_CHANNEL_ERROR,
                         g_io_channel_error_from_errno (errno), strerror (errno));
            g_io_channel_shutdown (ret, FALSE, NULL);
            g_io_channel_unref (ret);
            return NULL;
        }

        if (g_io_channel_set_encoding (ret, NULL, err) != G_IO_STATUS_NORMAL) {
            g_io_channel_shutdown (ret, FALSE, NULL);
            g_io_channel_unref (ret);
            return NULL;
        }

        if (created) {
            /* Write the header when we make a new file */
            if (g_io_channel_write_chars (ret, GPG_CONF_HEADER, -1, NULL, err) !=
                G_IO_STATUS_NORMAL
                || g_io_channel_flush (ret, err) != G_IO_STATUS_NORMAL) {
                g_io_channel_shutdown (ret, FALSE, NULL);
                g_io_channel_unref (ret);
                return NULL;
            }
        }
    }

    return ret;
}

#define HOME_PREFIX "\nHome: "

/* Discovers .gnupg home directory by running gpg */
static gboolean
parse_home_directory (gpgme_engine_info_t engine, GError ** err)
{
    gboolean found = FALSE;
    gchar *sout = NULL;
    gchar *serr = NULL;
    gchar *t;
    gchar *x;
    gint status;
    gboolean b;

    g_assert (engine);
    g_assert (engine->file_name);

    /* We run /usr/bin/gpg --version */
    t = g_strconcat (engine->file_name, " --version", NULL);
    b = g_spawn_command_line_sync (t, &sout, &serr, &status, err);
    g_free (t);

    if (b) {
        if (sout && WIFEXITED (status) && WEXITSTATUS (status) == 0) {
            /* Look for Home: */
            t = strstr (sout, HOME_PREFIX);
            if (t != NULL) {
                t += strlen (HOME_PREFIX);
                x = strchr (t, '\n');
                if (x != NULL && x != t) {
                    *x = 0;
                    g_strstrip (t);

                    gpg_homedir[0] = 0;

                    /* If it's not a rooted path then expand */
                    if (t[0] == '~') {
                        g_strlcpy (gpg_homedir, g_get_home_dir (),
                                   sizeof (gpg_homedir));
                        t++;
                    }

                    g_strlcat (gpg_homedir, t, sizeof (gpg_homedir));
                    found = TRUE;
                }
            }
        }

        if (!found)
            b = FALSE;
    }

    if (sout)
        g_free (sout);
    if (serr)
        g_free (serr);

    return b;
}

/* Initializes the gpg-options static info */
static gboolean
gpg_options_init (GError **err)
{
    if (!gpg_options_inited) {
        gpgme_error_t gerr;
        gpgme_engine_info_t engine;

        gerr = gpgme_get_engine_info (&engine);
        g_return_val_if_fail (GPG_IS_OK (gerr),
                              (seahorse_util_gpgme_to_error (gerr, err), FALSE));

        /* Look for the OpenPGP engine */
        while (engine && engine->protocol != GPGME_PROTOCOL_OpenPGP)
            engine = engine->next;

        /* 
         * Make sure it's the right version for us to be messing 
         * around with the configuration file.
         */
        g_return_val_if_fail (engine && engine->version && engine->file_name &&
                              g_str_has_prefix (engine->version, GPG_VERSION_PREFIX),
                              (seahorse_util_gpgme_to_error
                               (GPG_E (GPG_ERR_INV_ENGINE), err), FALSE));

        /* Now run the binary and read in the home directory */
        if (!parse_home_directory (engine, err))
            return FALSE;

        gpg_options_inited = TRUE;
    }

    return TRUE;
}

/**
 * seahorse_gpg_options_find
 * 
 * @option: The option to find
 * @value: Returns the value, or NULL when not found
 * @err: Returns an error value when errors
 * 
 * Find the value for a given option in the gpg config file.
 * Values without a value are returned as an empty string.
 * On success be sure to free *value after you're done with it. 
 * 
 * Returns: TRUE if success, FALSE if not
 **/
gboolean
seahorse_gpg_options_find (const gchar *option, gchar **value, GError **err)
{
    const gchar *options[2];

    options[0] = option;
    options[1] = NULL;

    return seahorse_gpg_options_find_vals (options, value, err);
}

/**
 * seahorse_gpg_options_find_vals
 * 
 * @option: null terminated array of option names
 * @value: An array of pointers for return values 
 * @err: Returns an error value when errors
 * 
 * Find the value for a given options in the gpg config file.
 * Values without a value are returned as an empty string.
 * On success be sure to free all *value after you're done 
 * with them. values should be at least as big as options
 * 
 * Returns: TRUE if success, FALSE if not
 **/
gboolean
seahorse_gpg_options_find_vals (const gchar *options[], gchar *values[],
                                GError **err)
{
    GError *e = NULL;
    GIOChannel *io;
    gchar *line = NULL;
    const gchar **opt;
    gchar *t;
    guint i;

    if (!gpg_options_init (err))
        return FALSE;

    /* Because we use err locally */
    if (!err)
        err = &e;

    io = open_config_file (TRUE, err);
    if (!io)
        return FALSE;

    /* Clear out all values */
    for (i = 0, opt = options; *opt != NULL; opt++, i++)
        values[i] = NULL;

    while (g_io_channel_read_line (io, &line, NULL, NULL, err) == G_IO_STATUS_NORMAL) {
        if (!line)
            continue;

        g_strstrip (line);

        /* Ignore comments and blank lines */
        if (line[0] != '#' && line[0] != 0) {
            for (i = 0, opt = options; *opt != NULL; opt++, i++) {
                if (g_str_has_prefix (line, *opt)) {
                    t = line + strlen (*opt);
                    if (t[0] == 0 || g_ascii_isspace (t[0])) {
                        /* 
                         * We found a value. Fill it in. The caller
                         * frees this stuff. Note that we don't short 
                         * circuit the search because for gpg options 
                         * can be specified multiple times, and the 
                         * last one wins.
                         */

                        g_free (values[i]);
                        values[i] = g_strdup (t);
                        g_strstrip (values[i]);
                        break;  /* Done with this line */
                    }
                }
            }
        }

        g_free (line);
        line = NULL;
    }

    g_io_channel_unref (io);
    g_free (line);

    return *err ? FALSE : TRUE;
}

/* Figure out needed changes to configuration file */
static gboolean
process_conf_edits (GIOChannel *io, GArray *lines, gint64 *position,
                    const gchar *options[], gchar *values[], GError **err)
{
    gboolean comment;
    const gchar **opt;
    gchar *t;
    gchar *n;
    gchar *line;
    gsize length;
    gint64 last = 0;
    gint64 pos = 0;
    gboolean ending = TRUE;
    int i;
    GIOStatus x;

    *position = -1;

    while ((x =
            g_io_channel_read_line (io, &line, &length, NULL,
                                    err)) == G_IO_STATUS_NORMAL) {
        if (length == 0) {
            g_assert (line == NULL);
            continue;
        }

        /* 
         * Does this line have an ending? 
         * We use this below when appending lines.
         */
        ending = (line[length - 1] == '\n');

        last = pos;
        pos += length;
        n = line;

        /* Don't use g_strstrip as we don't want to modify the line */
        while (*n && g_ascii_isspace (*n))
            n++;

        /* Ignore blank lines */
        if (n[0] != 0) {
            comment = FALSE;

            /* We look behind comments to see if we need to uncomment them */
            if (n[0] == '#') {
                n++;
                comment = TRUE;

                while (*n && g_ascii_isspace (*n))
                    n++;
            }

            for (i = 0, opt = options; *opt != NULL; opt++, i++) {
                if (!g_str_has_prefix (n, *opt))
                    continue;

                t = n + strlen (*opt);
                if (t[0] != 0 && !g_ascii_isspace (t[0]))
                    continue;

                /* Are we setting this value? */
                if (values[i]) {
                    /* At this point we're rewriting the line, so we 
                     * can modify the old line */
                    *t = 0;

                    /* A line with a value */
                    if (values[i][0])
                        n = g_strconcat (n, " ", values[i], "\n", NULL);

                    /* A setting without a value */
                    else
                        n = g_strconcat (n, "\n", NULL);

                    /* 
                     * We're done with this option, all other instances
                     * of it need to be commented out 
                     */
                    values[i] = NULL;
                }

                /* Otherwise we're removing the value */
                else if (!comment) {
                    n = g_strconcat ("# ", n, NULL);
                }

                g_free (line);
                line = n;

                /* Make note of where we need to write from */
                if (*position < 0)
                    *position = last;

                /* Done with this line */
                break;
            }
        }

        /* We've made changes so keep track of everything */
        if (*position >= 0)
            g_array_append_val (lines, line);

        /* No changes yet, so discard */
        else
            g_free (line);

        line = NULL;
    }

    if (*err != NULL)
        return FALSE;

    /* Append any that haven't been added but need to */
    for (i = 0, opt = options; *opt != NULL; *opt++, i++) {
        /* Are we setting this value? */
        if (values[i]) {
            /* If the last line didn't have an ending, then add one */
            if (!ending) {
                n = g_strdup ("\n");
                g_array_append_val (lines, n);
                ending = TRUE;
            }

            /* A line with a value */
            if (values[i][0])
                n = g_strconcat (*opt, " ", values[i], "\n", NULL);

            /* A setting without a value */
            else
                n = g_strconcat (*opt, "\n", NULL);

            g_array_append_val (lines, n);

            if (*position < 0)
                *position = pos;
        }
    }

    return TRUE;
}

/**
 * seahorse_gpg_options_change
 * 
 * @option: The option to change
 * @value: The value to change it to
 * @err: Returns an error value when errors
 * 
 * Changes the given option in the gpg config file.
 * If value is NULL, the option will be deleted. If you want
 * an empty value, set value to an empty string. 
 * 
 * Returns: TRUE if success, FALSE if not
 **/
gboolean
seahorse_gpg_options_change (const gchar *option, const gchar *value,
                             GError **err)
{
    const gchar *options[2];

    options[0] = option;
    options[1] = NULL;

    return seahorse_gpg_options_find_vals (options, (gchar **)&value, err);
}

/**
 * seahorse_gpg_options_change_vals
 * 
 * @option: null-terminated array of option names to change
 * @value: The values to change respective option to
 * @err: Returns an error value when errors
 * 
 * Changes the given option in the gpg config file.
 * If a value is NULL, the option will be deleted. If you want
 * an empty value, set value to an empty string. 
 * 
 * Returns: TRUE if success, FALSE if not
 **/
gboolean
seahorse_gpg_options_change_vals (const gchar *options[], gchar *values[],
                                  GError **err)
{
    GError *e = NULL;
    GIOChannel *io;
    GArray *lines;
    gint64 position = -1;
    const gchar *t;
    int i;
    off_t o;
    gsize written;

    if (!gpg_options_init (err))
        return FALSE;

    if (!err)
        err = &e;

    io = open_config_file (FALSE, err);
    if (!io)
        return FALSE;

    lines = g_array_new (FALSE, FALSE, sizeof (gchar *));

    /* Seek to beginning */
    if (g_io_channel_seek_position (io, 0LL, G_SEEK_SET, err) == G_IO_STATUS_NORMAL) {
        if (process_conf_edits (io, lines, &position, options, values, err)) {
            if (position >= 0 &&
                g_io_channel_seek_position (io, position, G_SEEK_SET,
                                            err) == G_IO_STATUS_NORMAL) {
                /* Write out each line past modified position */
                for (i = 0; i < lines->len; i++) {
                    t = g_array_index (lines, gchar *, i);
                    g_assert (t != NULL);

                    if (g_io_channel_write_chars (io, t, -1, &written, err) !=
                        G_IO_STATUS_NORMAL)
                        break;

                    position += written;
                }

                if (g_io_channel_flush (io, err) == G_IO_STATUS_NORMAL) {
                    /* We have to cut off the file in case we removed data */
                    o = (off_t) (position);
                    if (ftruncate (g_io_channel_unix_get_fd (io), o) == -1) {
                        g_set_error (err, G_IO_CHANNEL_ERROR,
                                     g_io_channel_error_from_errno (errno),
                                     strerror (errno));
                    }
                }
            }
        }
    }

    for (i = 0; i < lines->len; i++)
        g_free (g_array_index (lines, gchar *, i));

    g_array_free (lines, TRUE);
    g_io_channel_unref (io);

    return *err ? FALSE : TRUE;
}
