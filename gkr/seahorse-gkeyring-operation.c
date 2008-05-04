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
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <glib/gi18n.h>

#include "seahorse-gkeyring-operation.h"
#include "seahorse-util.h"
#include "seahorse-passphrase.h"

#include <gnome-keyring.h>

#ifndef DEBUG_OPERATION_ENABLE
#if _DEBUG
#define DEBUG_OPERATION_ENABLE 1
#else
#define DEBUG_OPERATION_ENABLE 0
#endif
#endif

#if DEBUG_OPERATION_ENABLE
#define DEBUG_OPERATION(x)  g_printerr x
#else
#define DEBUG_OPERATION(x)
#endif

/* -----------------------------------------------------------------------------
 * DEFINITIONS
 */
 
typedef struct _SeahorseGKeyringOperationPrivate {
    
    gpointer request;
    GQuark keyid;
    
} SeahorseGKeyringOperationPrivate;

enum {
    PROP_0,
    PROP_KEY_SOURCE
};

#define SEAHORSE_GKEYRING_OPERATION_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SEAHORSE_TYPE_GKEYRING_OPERATION, SeahorseGKeyringOperationPrivate))

/* TODO: This is just nasty. Gotta get rid of these weird macros */
IMPLEMENT_OPERATION_PROPS(GKeyring, gkeyring)

    g_object_class_install_property (gobject_class, PROP_KEY_SOURCE,
        g_param_spec_object ("key-source", "GKeyring Source", "Key source this operation works on.", 
                             SEAHORSE_TYPE_GKEYRING_SOURCE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY ));

    g_type_class_add_private (gobject_class, sizeof (SeahorseGKeyringOperationPrivate));

END_IMPLEMENT_OPERATION_PROPS

/* -----------------------------------------------------------------------------
 * HELPERS 
 */

static gboolean
check_operation_result (SeahorseGKeyringOperation *gop, GnomeKeyringResult result)
{
    GError *err = NULL;
    gboolean success;
    
    /* This only gets called when we cancel, so ignore */
    if (result == GNOME_KEYRING_RESULT_CANCELLED)
        return FALSE;
    
    success = seahorse_gkeyring_operation_parse_error (result, &err);
    g_assert (!success || !err);
    
    seahorse_operation_mark_done (SEAHORSE_OPERATION (gop), FALSE, err);
    return success;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gkeyring_operation_init (SeahorseGKeyringOperation *sop)
{

}

static void 
seahorse_gkeyring_operation_set_property (GObject *gobject, guint prop_id, 
                                          const GValue *value, GParamSpec *pspec)
{
    SeahorseGKeyringOperation *gop = SEAHORSE_GKEYRING_OPERATION (gobject);
    
    switch (prop_id) {
    case PROP_KEY_SOURCE:
        if (gop->gsrc)
            g_object_unref (gop->gsrc);
        gop->gsrc = SEAHORSE_GKEYRING_SOURCE (g_object_ref (g_value_get_object (value)));
        break;
    }
    
}

static void 
seahorse_gkeyring_operation_get_property (GObject *gobject, guint prop_id, 
                                          GValue *value, GParamSpec *pspec)
{
    SeahorseGKeyringOperation *gop = SEAHORSE_GKEYRING_OPERATION (gobject);
    
    switch (prop_id) {
    case PROP_KEY_SOURCE:
        g_value_set_object (value, gop->gsrc);
        break;
    }
}

static void 
seahorse_gkeyring_operation_dispose (GObject *gobject)
{
    SeahorseGKeyringOperation *gop = SEAHORSE_GKEYRING_OPERATION (gobject);
    SeahorseGKeyringOperationPrivate *pv = SEAHORSE_GKEYRING_OPERATION_GET_PRIVATE (gop);

    if (seahorse_operation_is_running (SEAHORSE_OPERATION (gop)))
        seahorse_gkeyring_operation_cancel (SEAHORSE_OPERATION (gop));
    g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (gop)));
    
    if (gop->gsrc)
        g_object_unref (gop->gsrc);
    gop->gsrc = NULL;
    
    /* The above cancel should have stopped this */
    g_assert (pv->request == NULL);
    
    G_OBJECT_CLASS (operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_gkeyring_operation_finalize (GObject *gobject)
{
    SeahorseGKeyringOperation *gop = SEAHORSE_GKEYRING_OPERATION (gobject);
    SeahorseGKeyringOperationPrivate *pv = SEAHORSE_GKEYRING_OPERATION_GET_PRIVATE (gop);
    
    g_assert (!gop->gsrc);
    g_assert (!pv->request);
    
    G_OBJECT_CLASS (operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_gkeyring_operation_cancel (SeahorseOperation *operation)
{
    SeahorseGKeyringOperation *sop = SEAHORSE_GKEYRING_OPERATION (operation);    
    SeahorseGKeyringOperationPrivate *pv = SEAHORSE_GKEYRING_OPERATION_GET_PRIVATE (sop);

    if (pv->request)
        gnome_keyring_cancel_request (pv->request);
    pv->request = NULL;
    
    if (seahorse_operation_is_running (operation))
        seahorse_operation_mark_done (operation, TRUE, NULL);    
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

gboolean
seahorse_gkeyring_operation_parse_error (GnomeKeyringResult result, GError **err)
{
    static GQuark errorq = 0;
    const gchar *message = NULL;

    if (result == GNOME_KEYRING_RESULT_OK)
        return TRUE;
    
    /* These should be handled in the callbacks */
    g_assert (result != GNOME_KEYRING_RESULT_CANCELLED);
    
    /* An error mark it as such */
    switch (result) {
    case GNOME_KEYRING_RESULT_DENIED:
        message = _("Access to the key ring was denied");
        break;
    case GNOME_KEYRING_RESULT_NO_KEYRING_DAEMON:
        message = _("The gnome-keyring daemon is not running");
        break;
    case GNOME_KEYRING_RESULT_ALREADY_UNLOCKED:
        message = _("The key ring has already been unlocked");
        break;
    case GNOME_KEYRING_RESULT_NO_SUCH_KEYRING:
        message = _("No such key ring exists");
        break;
    case GNOME_KEYRING_RESULT_IO_ERROR:
        message = _("Couldn't communicate with key ring daemon");
        break;
    case GNOME_KEYRING_RESULT_ALREADY_EXISTS:
        message = _("The item already exists");
        break;
    case GNOME_KEYRING_RESULT_BAD_ARGUMENTS:
        g_warning ("bad arguments passed to gnome-keyring API");
        /* Fall through */
    default:
        message = _("Internal error accessing gnome-keyring");
        break;
    }
    
    if (!errorq) 
        errorq = g_quark_from_static_string ("seahorse-gnome-keyring");
    
    g_set_error (err, errorq, result, "%s", message);
    return FALSE;
}

/* -----------------------------------------------------------------------------
 * UPDATE INFO OPERATION
 */

static void 
basic_operation_done (GnomeKeyringResult result, SeahorseGKeyringOperation *gop)
{
    SeahorseGKeyringOperationPrivate *pv;

    if (!check_operation_result (gop, result))
        return;
    
    pv = SEAHORSE_GKEYRING_OPERATION_GET_PRIVATE (gop);

    /* When operation is successful reload the key */
    g_return_if_fail (pv->keyid != 0);
    seahorse_key_source_load_async (SEAHORSE_KEY_SOURCE (gop->gsrc), pv->keyid);
}

SeahorseOperation*
seahorse_gkeyring_operation_update_info (SeahorseGKeyringItem *git, GnomeKeyringItemInfo *info)
{
    SeahorseKeySource *sksrc;
    SeahorseGKeyringOperation *gop;
    SeahorseGKeyringOperationPrivate *pv;
    const gchar *keyring_name;
    
    g_return_val_if_fail (SEAHORSE_IS_GKEYRING_ITEM (git), NULL);
    
    sksrc = seahorse_key_get_source (SEAHORSE_KEY (git));
    g_return_val_if_fail (SEAHORSE_IS_GKEYRING_SOURCE (sksrc), NULL);
    
    gop = g_object_new (SEAHORSE_TYPE_GKEYRING_OPERATION, 
                        "key-source", SEAHORSE_GKEYRING_SOURCE (sksrc), NULL);
    pv = SEAHORSE_GKEYRING_OPERATION_GET_PRIVATE (gop);
    
    keyring_name = seahorse_gkeyring_source_get_keyring_name (SEAHORSE_GKEYRING_SOURCE (sksrc));
    pv->keyid = seahorse_key_get_keyid (SEAHORSE_KEY (git));
    
    /* Start actual save request */
    pv->request = gnome_keyring_item_set_info (keyring_name, git->item_id, info, 
                                               (GnomeKeyringOperationDoneCallback)basic_operation_done,
                                               gop, NULL);
    g_return_val_if_fail (pv->request, NULL);
    
    seahorse_operation_mark_start (SEAHORSE_OPERATION (gop));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (gop), _("Saving item..."), -1);

    return SEAHORSE_OPERATION (gop);
}

SeahorseOperation*
seahorse_gkeyring_operation_update_acl (SeahorseGKeyringItem *git, GList *acl)
{
    SeahorseKeySource *sksrc;
    SeahorseGKeyringOperation *gop;
    SeahorseGKeyringOperationPrivate *pv;
    const gchar *keyring_name;
    
    g_return_val_if_fail (SEAHORSE_IS_GKEYRING_ITEM (git), NULL);
    
    sksrc = seahorse_key_get_source (SEAHORSE_KEY (git));
    g_return_val_if_fail (SEAHORSE_IS_GKEYRING_SOURCE (sksrc), NULL);
    
    gop = g_object_new (SEAHORSE_TYPE_GKEYRING_OPERATION, 
                        "key-source", SEAHORSE_GKEYRING_SOURCE (sksrc), NULL);
    pv = SEAHORSE_GKEYRING_OPERATION_GET_PRIVATE (gop);
    
    keyring_name = seahorse_gkeyring_source_get_keyring_name (SEAHORSE_GKEYRING_SOURCE (sksrc));
    pv->keyid = seahorse_key_get_keyid (SEAHORSE_KEY (git));
    
    /* Start actual save request */
    pv->request = gnome_keyring_item_set_acl (keyring_name, git->item_id, acl, 
                                              (GnomeKeyringOperationDoneCallback)basic_operation_done,
                                              gop, NULL);
    g_return_val_if_fail (pv->request, NULL);
    
    seahorse_operation_mark_start (SEAHORSE_OPERATION (gop));
    seahorse_operation_mark_progress (SEAHORSE_OPERATION (gop), _("Saving item..."), -1);

    return SEAHORSE_OPERATION (gop);
}

