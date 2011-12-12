/*
 * Seahorse
 *
 * Copyright (C) 2004,2005 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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

#include "seahorse-lockable.h"

typedef SeahorseLockableIface SeahorseLockableInterface;

G_DEFINE_INTERFACE (SeahorseLockable, seahorse_lockable, G_TYPE_OBJECT);

static void
seahorse_lockable_default_init (SeahorseLockableIface *iface)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		initialized = TRUE;

		g_object_interface_install_property (iface,
		               g_param_spec_boolean ("lockable", "Lockable", "Is actually lockable",
		                                      FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

		g_object_interface_install_property (iface,
		               g_param_spec_boolean ("unlockable", "Unlockable", "Is actually unlockable",
		                                      FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
	}
}

void
seahorse_lockable_lock_async (SeahorseLockable *lockable,
                              GTlsInteraction *interaction,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	SeahorseLockableIface *iface;

	g_return_if_fail (SEAHORSE_IS_LOCKABLE (lockable));
	g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = SEAHORSE_LOCKABLE_GET_INTERFACE (lockable);
	g_return_if_fail (iface->lock_async != NULL);

	(iface->lock_async) (lockable, interaction, cancellable, callback, user_data);
}

gboolean
seahorse_lockable_lock_finish (SeahorseLockable *lockable,
                               GAsyncResult *result,
                               GError **error)
{
	SeahorseLockableIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_LOCKABLE (lockable), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	iface = SEAHORSE_LOCKABLE_GET_INTERFACE (lockable);
	g_return_val_if_fail (iface->lock_finish != NULL, FALSE);

	return (iface->lock_finish) (lockable, result, error);
}


void
seahorse_lockable_unlock_async (SeahorseLockable *lockable,
                                GTlsInteraction *interaction,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	SeahorseLockableIface *iface;

	g_return_if_fail (SEAHORSE_IS_LOCKABLE (lockable));
	g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	iface = SEAHORSE_LOCKABLE_GET_INTERFACE (lockable);
	g_return_if_fail (iface->unlock_async != NULL);

	(iface->unlock_async) (lockable, interaction, cancellable, callback, user_data);
}

gboolean
seahorse_lockable_unlock_finish (SeahorseLockable *lockable,
                                 GAsyncResult *result,
                                 GError **error)
{
	SeahorseLockableIface *iface;

	g_return_val_if_fail (SEAHORSE_IS_LOCKABLE (lockable), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	iface = SEAHORSE_LOCKABLE_GET_INTERFACE (lockable);
	g_return_val_if_fail (iface->unlock_finish != NULL, FALSE);

	return (iface->unlock_finish) (lockable, result, error);
}

gboolean
seahorse_lockable_can_unlock (gpointer object)
{
	gboolean unlockable;

	if (!SEAHORSE_IS_LOCKABLE (object))
		return FALSE;

	g_object_get (object, "unlockable", &unlockable, NULL);
	return unlockable;
}

gboolean
seahorse_lockable_can_lock (gpointer object)
{
	gboolean lockable;

	if (!SEAHORSE_IS_LOCKABLE (object))
		return FALSE;

	g_object_get (object, "lockable", &lockable, NULL);
	return lockable;
}
