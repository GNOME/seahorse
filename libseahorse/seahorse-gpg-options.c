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

#include "seahorse-gpgmex.h"
#include "seahorse-util.h"
#include "seahorse-context.h"
#include "seahorse-gpg-options.h"

#define  GPG_CONF_HEADER    "# FILE CREATED BY SEAHORSE\n\n"
#define  GPG_VERSION_PREFIX   "1."

static gchar gpg_homedir[MAXPATHLEN];
static gboolean gpg_options_inited = FALSE;

static gboolean
create_file (const gchar *file, mode_t mode, GError **err)
{
    int fd;
    g_assert (err && !*err);
    
    if ((fd = open (file, O_CREAT | O_TRUNC | O_WRONLY, mode)) == -1) {
        g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                     strerror (errno));     
		return FALSE;
    }

	/* Write the header when we make a new file */
	if (write (fd, GPG_CONF_HEADER, strlen (GPG_CONF_HEADER)) == -1) {
        g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                     strerror (errno));     
	}
	
	close (fd);
    return *err ? FALSE : TRUE;
}

/* Finds relevant configuration file, creates if not found */
static gchar *
find_config_file (gboolean read, GError **err)
{
    gchar *conf = NULL;

    g_assert (gpg_options_inited);
    g_assert (!err || !*err);

    /* Check for and open ~/.gnupg/gpg.conf */
    conf = g_strconcat (gpg_homedir, "/gpg.conf", NULL);
    if (g_file_test (conf, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) 
        return conf;
    g_free (conf);
    	
    /* Check for and open ~/.gnupg/options */
    conf = g_strconcat (gpg_homedir, "/options", NULL);
    if (g_file_test (conf, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) 
        return conf;
    g_free (conf);

    /* Make sure directory exists */
    if (!g_file_test (gpg_homedir, G_FILE_TEST_EXISTS)) {
        if (mkdir (gpg_homedir, 0700) == -1) {
            g_set_error (err, G_IO_CHANNEL_ERROR, 
                         g_io_channel_error_from_errno (errno),
                         strerror (errno));     
            return NULL;
        }
    }

    /* For writers just return the file name */
    conf = g_strconcat (gpg_homedir, "/gpg.conf", NULL);	
    if (!read)
        return conf;

    /* ... for readers we create ~/.gnupg/gpg.conf */
    if (create_file (conf, 0600, err))
        return conf;
    g_free (conf);
	
    return NULL;
}	

static GArray*
read_config_file (GError **err)
{
    GIOChannel *io;
    GError *e = NULL;
    GArray *ret;
    gchar *conf;
    gchar *line;

    g_assert (!err || !*err);
    if (!err)
        err = &e;
    
    conf = find_config_file (TRUE, err);
    if (conf == NULL)
        return NULL;

    io = g_io_channel_new_file (conf, "r", err);
    g_free (conf);

    if (io == NULL)
        return NULL;        

    g_io_channel_set_encoding (io, NULL, NULL);    
    ret = g_array_new (FALSE, TRUE, sizeof (char*));

    while (g_io_channel_read_line (io, &line, NULL, NULL, err) == G_IO_STATUS_NORMAL)
        g_array_append_val (ret, line);

    g_io_channel_unref (io);

    if (*err != NULL) {
        g_array_free (ret, TRUE);
        return NULL;
    }
    
    return ret;
}    

static gboolean
write_config_file (GArray *lines, GError **err)
{
    GError *e = NULL;
    gchar *conf;
    guint i;
    int fd;

    g_assert (!err || !*err);
    if (!err)
        err = &e;
    
    conf = find_config_file (FALSE, err);
    if (conf == NULL)
        return FALSE;

    fd = open (conf, O_CREAT | O_TRUNC | O_WRONLY, 0700);
    g_free (conf);

    if (fd == -1) {
        g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                     strerror (errno));     
		return FALSE;
    }

    for (i = 0; i < lines->len; i++) {
        const gchar *line = g_array_index (lines, const gchar*, i);
        g_assert (line != NULL);        

        if (write (fd, line, strlen (line)) == -1) {
            g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                         strerror (errno));     
            break;
        }
    }

    close (fd);    
    
    return *err ? FALSE : TRUE;
}

static void
free_string_array (GArray *lines)
{
    guint i;
    for (i = 0; i < lines->len; i++)
        g_free (g_array_index (lines, gchar*, i));
    g_array_free (lines, TRUE);
}

#define HOME_PREFIX "\nHome: "

/* Discovers .gnupg home directory by running gpg */
static gboolean
parse_home_directory (gpgme_engine_info_t engine, GError **err)
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
 * seahorse_gpg_homedir
 * 
 * Returns: The home dir that GPG uses for it's keys and configuration
 **/
const gchar*
seahorse_gpg_homedir ()
{
    /* THis shouldn't normally fail, and as such we return an invalid 
     * directory to avoid NULL memory access */
    g_return_val_if_fail (gpg_options_init (NULL), "/invalid/gpg/dir");
    return gpg_homedir;
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
    GArray *lines;
    const gchar **opt;
    gchar *line;
    gchar *t;
    guint i, j;
    
    g_assert (!err || !*err);
    if (!err)
        err = &e;

    if (!gpg_options_init (err))
        return FALSE;
    
    lines = read_config_file (err);
    if (!lines)
        return FALSE;

    /* Clear out all values */
    for (i = 0, opt = options; *opt != NULL; opt++, i++)
        values[i] = NULL;

    for (j = 0; j < lines->len; j++) {
        line = g_array_index (lines, gchar*, j);
        g_assert (line != NULL);        

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
    }

    free_string_array (lines);

    return *err ? FALSE : TRUE;
}

/* Figure out needed changes to configuration file */
static void
process_conf_edits (GArray *lines, const gchar *options[], gchar *values[])
{
    gboolean comment;
    const gchar **opt;
    gchar *t;
    gchar *n;
    gchar *line;
    gsize length;
    gboolean ending = TRUE;
    guint i, j;

    for (j = 0; j < lines->len; j++) {
        line = g_array_index (lines, gchar*, j);
        g_assert (line != NULL);        
        length = strlen(line);
        
        /* 
         * Does this line have an ending? 
         * We use this below when appending lines.
         */
        ending = (line[length - 1] == '\n');
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

                line = n;

                /* Done with this line */
                break;
            }
        }

        if (g_array_index (lines, gchar*, j) != line) {
            g_free (g_array_index (lines, gchar*, j));
            g_array_index (lines, gchar*, j) = line;
        }
    }

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
        }
    }
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

    return seahorse_gpg_options_change_vals (options, (gchar **)&value, err);
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
    GArray *lines;

    g_assert (!err || !*err);
    if (!err)
        err = &e;

    if (!gpg_options_init (err))
        return FALSE;

    lines = read_config_file (err);
    if (!lines)
        return FALSE;

    process_conf_edits (lines, options, values);
    
    write_config_file (lines, err);
    free_string_array (lines);
    
    return *err ? FALSE : TRUE;
}
