/*
 * Seahorse
 *
 * Copyright (C) 2003 Stefan Walter
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

#include "config.h"

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gpgme.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "seahorse-util.h"

#include "pgp/seahorse-gpgme.h"
#include "pgp/seahorse-gpg-options.h"

#define  GPG_CONF_HEADER    "# FILE CREATED BY SEAHORSE\n\n"
#define  GPG_VERSION_PREFIX1   "1."
#define  GPG_VERSION_PREFIX2   "2."

static gchar *gpg_homedir;
static gboolean gpg_options_inited = FALSE;

static gboolean
create_file (const gchar *file, mode_t mode, GError **err)
{
    int fd;
    g_assert (err && !*err);
    
    if ((fd = open (file, O_CREAT | O_TRUNC | O_WRONLY, mode)) == -1) {
        g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                     "%s", g_strerror (errno));
        return FALSE;
    }

    /* Write the header when we make a new file */
    if (write (fd, GPG_CONF_HEADER, strlen (GPG_CONF_HEADER)) == -1) {
        g_set_error (err, G_IO_CHANNEL_ERROR, g_io_channel_error_from_errno (errno),
                     "%s", strerror (errno));
    }
    
    close (fd);
    return *err ? FALSE : TRUE;
}

/* Finds relevant configuration file, creates if not found */
static gchar *
find_config_file (gboolean read, GError **err)
{
	gchar *conf = NULL;

	g_assert (!err || !*err);

	if (!gpg_options_inited)
		return NULL;
	if (!gpg_homedir)
		return NULL;

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
                         "%s", strerror (errno));
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
    GError *e = NULL;
    gboolean ret;
    GArray *array;
    gchar *conf, *contents;
    gchar **lines, **l;

    g_assert (!err || !*err);
    if (!err)
        err = &e;
    
    conf = find_config_file (TRUE, err);
    if (conf == NULL)
        return NULL;
    
    ret = g_file_get_contents (conf, &contents, NULL, err);
    g_free (conf);
    
    if (!ret)
        return FALSE;
        
    lines = g_strsplit (contents, "\n", -1);
    g_free (contents);
    
    array = g_array_new (TRUE, TRUE, sizeof (gchar**));
    for (l = lines; *l; l++)
        g_array_append_val (array, *l);
    
    /* We took ownership of the individual lines */
    g_free (lines);
    return array;
}    

static gboolean
write_config_file (GArray *array, GError **err)
{
    GError *e = NULL;
    gchar *conf, *contents;

    g_assert (!err || !*err);
    if (!err)
        err = &e;
    
    conf = find_config_file (FALSE, err);
    if (conf == NULL)
        return FALSE;

    contents = g_strjoinv ("\n", (gchar**)(array->data));
    seahorse_util_write_file_private (conf, contents, err);
    g_free (contents);

    return *err ? FALSE : TRUE;
}

static void
free_string_array (GArray *array)
{
    gchar** lines = (gchar**)g_array_free (array, FALSE);
    g_strfreev (lines);
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

                    g_free (gpg_homedir);

                    /* If it's not a rooted path then expand */
                    if (t[0] == '~') {
                        gpg_homedir = g_strconcat (g_get_home_dir(), ++t, NULL);
                    } else {
                        gpg_homedir = g_strdup (t);
                    }
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
	gpgme_error_t gerr;
	gpgme_engine_info_t engine;

	if (gpg_options_inited)
		return TRUE;

	gerr = gpgme_get_engine_info (&engine);
	if (seahorse_gpgme_propagate_error (gerr, err))
		return FALSE;

	/* Look for the OpenPGP engine */
	while (engine && engine->protocol != GPGME_PROTOCOL_OpenPGP)
		engine = engine->next;

	/*
	 * Make sure it's the right version for us to be messing
	 * around with the configuration file.
	 */
	if (!engine || !engine->version || !engine->file_name ||
	    !(g_str_has_prefix (engine->version, GPG_VERSION_PREFIX1) ||
	      g_str_has_prefix (engine->version, GPG_VERSION_PREFIX2))) {
		seahorse_gpgme_propagate_error (GPG_E (GPG_ERR_INV_ENGINE), err);
		return FALSE;
	}

	/* Now run the binary and read in the home directory */
	if (!parse_home_directory (engine, err))
		return FALSE;

	gpg_options_inited = TRUE;
	return TRUE;
}

/**
 * seahorse_gpg_homedir
 * 
 * Returns: The home dir that GPG uses for it's keys and configuration
 **/
const gchar *
seahorse_gpg_homedir (void)
{
	if (!gpg_options_init (NULL))
		return NULL;
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
    gchar *t;
    gchar *n;
    gchar *line;
    guint i, j;

    for (j = 0; j < lines->len; j++) {
        line = g_array_index (lines, gchar*, j);
        g_assert (line != NULL);        

        /* 
         * Does this line have an ending? 
         * We use this below when appending lines.
         */
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

            for (i = 0; options[i] != NULL; i++) {
                if (!g_str_has_prefix (n, options[i]))
                    continue;

                t = n + strlen (options[i]);
                if (t[0] != 0 && !g_ascii_isspace (t[0]))
                    continue;

                /* Are we setting this value? */
                if (values[i]) {
                    /* At this point we're rewriting the line, so we 
                     * can modify the old line */
                    *t = 0;

                    /* A line with a value */
                    if (values[i][0])
                        n = g_strconcat (n, " ", values[i], NULL);

                    /* A setting without a value */
                    else
                        n = g_strdup (n);

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
    for (i = 0; options[i] != NULL; i++) {
        /* Are we setting this value? */
        if (values[i]) {

            /* A line with a value */
            if (values[i][0])
                n = g_strconcat (options[i], " ", values[i], NULL);

            /* A setting without a value */
            else
                n = g_strdup (options[i]);

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
