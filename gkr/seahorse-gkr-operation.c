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

#include "seahorse-gkr-operation.h"
#include "seahorse-util.h"
#include "seahorse-passphrase.h"

#include <gnome-keyring.h>

/* -----------------------------------------------------------------------------
 * DEFINITIONS
 */
 
struct _SeahorseGkrOperationPrivate {
	gpointer request;
	SeahorseObject *object;   
};

G_DEFINE_TYPE (SeahorseGkrOperation, seahorse_gkr_operation, SEAHORSE_TYPE_OPERATION);

/* -----------------------------------------------------------------------------
 * HELPERS 
 */

static gboolean
check_operation_result (SeahorseGkrOperation *self, GnomeKeyringResult result)
{
    GError *err = NULL;
    gboolean success;
    
    /* This only gets called when we cancel, so ignore */
    if (result == GNOME_KEYRING_RESULT_CANCELLED)
        return FALSE;
    
    success = seahorse_gkr_operation_parse_error (result, &err);
    g_assert (!success || !err);
    
    seahorse_operation_mark_done (SEAHORSE_OPERATION (self), FALSE, err);
    return success;
}

/* -----------------------------------------------------------------------------
 * OBJECT 
 */

static void 
seahorse_gkr_operation_init (SeahorseGkrOperation *self)
{
	self->pv = G_TYPE_INSTANCE_GET_PRIVATE (self, SEAHORSE_TYPE_GKR_OPERATION, SeahorseGkrOperationPrivate);

}

static void 
seahorse_gkr_operation_dispose (GObject *gobject)
{
	SeahorseGkrOperation *self = SEAHORSE_GKR_OPERATION (gobject);

	if (seahorse_operation_is_running (SEAHORSE_OPERATION (self)))
		seahorse_operation_cancel (SEAHORSE_OPERATION (self));
	g_assert (!seahorse_operation_is_running (SEAHORSE_OPERATION (self)));
    
	if (self->pv->object)
		g_object_unref (self->pv->object);
	self->pv->object = NULL;
    
	/* The above cancel should have stopped this */
	g_assert (self->pv->request == NULL);
    
	G_OBJECT_CLASS (seahorse_gkr_operation_parent_class)->dispose (gobject);  
}

static void 
seahorse_gkr_operation_finalize (GObject *gobject)
{
	SeahorseGkrOperation *self = SEAHORSE_GKR_OPERATION (gobject);
    
	g_assert (!self->pv->request);
    
	G_OBJECT_CLASS (seahorse_gkr_operation_parent_class)->finalize (gobject);  
}

static void 
seahorse_gkr_operation_cancel (SeahorseOperation *operation)
{
	SeahorseGkrOperation *self = SEAHORSE_GKR_OPERATION (operation);    

	if (self->pv->request)
		gnome_keyring_cancel_request (self->pv->request);
	self->pv->request = NULL;
    
	if (seahorse_operation_is_running (operation))
		seahorse_operation_mark_done (operation, TRUE, NULL);    
}

static void
seahorse_gkr_operation_class_init (SeahorseGkrOperationClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
	seahorse_gkr_operation_parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (SeahorseGkrOperationPrivate));

	gobject_class->dispose = seahorse_gkr_operation_dispose;
	gobject_class->finalize = seahorse_gkr_operation_finalize;
	
	SEAHORSE_OPERATION_CLASS (klass)->cancel = seahorse_gkr_operation_cancel;
}

/* -----------------------------------------------------------------------------
 * PUBLIC 
 */

gboolean
seahorse_gkr_operation_parse_error (GnomeKeyringResult result, GError **err)
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
basic_operation_done (GnomeKeyringResult result, SeahorseGkrOperation *self)
{
	g_assert (SEAHORSE_IS_GKR_OPERATION (self));
	self->pv->request = NULL;

	if (!check_operation_result (self, result))
		return;
    
	/* When operation is successful reload the key */
	seahorse_object_refresh (SEAHORSE_OBJECT (self->pv->object));
}

SeahorseOperation*
seahorse_gkr_operation_update_info (SeahorseGkrItem *item, GnomeKeyringItemInfo *info)
{
	SeahorseGkrOperation *self;
    
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (item), NULL);
    
	self = g_object_new (SEAHORSE_TYPE_GKR_OPERATION, NULL);
    
	g_object_ref (item);
	self->pv->object = SEAHORSE_OBJECT (item);
    
	/* Start actual save request */
	g_object_ref (self);
	self->pv->request = gnome_keyring_item_set_info (seahorse_gkr_item_get_keyring_name (item),
	                                                 seahorse_gkr_item_get_item_id (item), info, 
	                                                 (GnomeKeyringOperationDoneCallback)basic_operation_done,
	                                                 self, g_object_unref);
	g_return_val_if_fail (self->pv->request, NULL);
    
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (self), _("Saving item..."), -1);

	return SEAHORSE_OPERATION (self);
}

SeahorseOperation*
seahorse_gkr_operation_update_acl (SeahorseGkrItem *item, GList *acl)
{
	SeahorseGkrOperation *self;
    
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (item), NULL);
    
	self = g_object_new (SEAHORSE_TYPE_GKR_OPERATION, NULL);

	g_object_ref (item);
	self->pv->object = SEAHORSE_OBJECT (item);
    
	/* Start actual save request */
	g_object_ref (self);
	self->pv->request = gnome_keyring_item_set_acl (seahorse_gkr_item_get_keyring_name (item), 
	                                                seahorse_gkr_item_get_item_id (item), acl, 
	                                                (GnomeKeyringOperationDoneCallback)basic_operation_done,
	                                                self, g_object_unref);
	g_return_val_if_fail (self->pv->request, NULL);
    
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (self), _("Saving item..."), -1);

	return SEAHORSE_OPERATION (self);
}

static void 
delete_operation_done (GnomeKeyringResult result, SeahorseGkrOperation *self)
{
	g_assert (SEAHORSE_IS_GKR_OPERATION (self));

	self->pv->request = NULL;
	if (check_operation_result (self, result))
		seahorse_context_remove_object (NULL, self->pv->object);
}

SeahorseOperation*
seahorse_gkr_operation_delete_item (SeahorseGkrItem *item)
{
	SeahorseGkrOperation *self;
	    
	g_return_val_if_fail (SEAHORSE_IS_GKR_ITEM (item), NULL);
	    
	self = g_object_new (SEAHORSE_TYPE_GKR_OPERATION, NULL);
	    
	g_object_ref (item);
	self->pv->object = SEAHORSE_OBJECT (item);
	    
	/* Start actual save request */
	g_object_ref (self);
	self->pv->request = gnome_keyring_item_delete (seahorse_gkr_item_get_keyring_name (item), 
	                                               seahorse_gkr_item_get_item_id (item), 
	                                               (GnomeKeyringOperationDoneCallback)delete_operation_done,
	                                               self, g_object_unref);
	g_return_val_if_fail (self->pv->request, NULL);
	
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (self), _("Deleting item..."), -1);
	
	return SEAHORSE_OPERATION (self);
}

SeahorseOperation*
seahorse_gkr_operation_delete_keyring (SeahorseGkrKeyring *keyring)
{
	SeahorseGkrOperation *self;
	    
	g_return_val_if_fail (SEAHORSE_IS_GKR_KEYRING (keyring), NULL);
	    
	self = g_object_new (SEAHORSE_TYPE_GKR_OPERATION, NULL);
	
	g_object_ref (keyring);
	self->pv->object = SEAHORSE_OBJECT (keyring);
	    
	/* Start actual save request */
	g_object_ref (self);
	self->pv->request = gnome_keyring_delete (seahorse_gkr_keyring_get_name (keyring), 
	                                          (GnomeKeyringOperationDoneCallback)delete_operation_done,
	                                          self, g_object_unref);
	g_return_val_if_fail (self->pv->request, NULL);
	
	seahorse_operation_mark_start (SEAHORSE_OPERATION (self));
	seahorse_operation_mark_progress (SEAHORSE_OPERATION (self), _("Deleting keyring..."), -1);
	
	return SEAHORSE_OPERATION (self);
}
