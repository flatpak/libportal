/*
 * Copyright (C) 2018, Matthias Clasen
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.0 of the
 * License.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "config.h"

#include "notification.h"
#include "portal-private.h"

static void
action_invoked (GDBusConnection *bus,
                const char *sender_name,
                const char *object_path,
                const char *interface_name,
                const char *signal_name,
                GVariant *parameters,
                gpointer data)
{
  XdpPortal *portal = data;
  const char *id;
  const char *action;
  g_autoptr(GVariant) parameter = NULL;

  g_variant_get (parameters, "(&s&s@av)", &id, &action, &parameter);

  g_signal_emit_by_name (portal, "notification-action-invoked",
                         id, action, parameter);
}

static void
ensure_action_invoked_connection (XdpPortal *portal)
{
  if (portal->action_invoked_signal == 0)
    portal->action_invoked_signal =
       g_dbus_connection_signal_subscribe (portal->bus,
                                           PORTAL_BUS_NAME,
                                           "org.freedesktop.portal.Notification",
                                           "ActionInvoked",
                                           PORTAL_OBJECT_PATH,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                           action_invoked,
                                           portal,
                                           NULL);
}

typedef struct {
  XdpPortal *portal;
  GAsyncReadyCallback callback;
  gpointer data;
} CallDoneData;

static void
call_done (GObject *source,
           GAsyncResult *result,
           gpointer data)
{
  CallDoneData *call_done_data = data;

  call_done_data->callback (G_OBJECT (call_done_data->portal), result, call_done_data->data);

  g_object_unref (call_done_data->portal);
  g_free (call_done_data);
}

/**
 * xdp_portal_add_notification:
 * @portal: a [class@Portal]
 * @id: unique ID for the notification
 * @notification: a [struct@GLib.Variant] dictionary with the content of the notification
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Sends a desktop notification.
 *
 * The following keys may be present in @notification:
 *
 * - title `s`: a user-visible string to display as title
 * - body `s`: a user-visible string to display as body
 * - icon `v`: a serialized icon (in the format produced by [method@Gio.Icon.serialize])
 * - priority `s`: "low", "normal", "high" or "urgent"
 * - default-action `s`: name of an action that
 *     will be activated when the user clicks on the notification
 * - default-action-target `v`: target parameter to send along when
 *     activating the default action.
 * - buttons `aa{sv}`: array of serialized buttons
 *
 * Each serialized button is a dictionary with the following supported keys:
 *
 * - label `s`: user-visible lable for the button. Mandatory
 * - action `s`: name of an action that will be activated when
 *     the user clicks on the button. Mandatory
 * - target `v`: target parameter to send along when activating
 *     the button
 *
 * Actions with a prefix of "app." are assumed to be exported by the
 * application and will be activated via the org.freedesktop.Application
 * interface, others are activated by emitting the
 * [signal@Portal::notification-action-invoked] signal.
 *
 * It is the callers responsibility to ensure that the ID is unique
 * among all notifications.
 *
 * To withdraw a notification, use [method@Portal.remove_notification].
 */
void
xdp_portal_add_notification (XdpPortal *portal,
                             const char *id,
                             GVariant *notification,
                             XdpNotificationFlags flags,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer data)
{
  GAsyncReadyCallback call_done_cb = NULL;
  CallDoneData *call_done_data = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_NOTIFICATION_FLAG_NONE);

  ensure_action_invoked_connection (portal);

  if (callback)
    {
      call_done_cb = call_done; 
      call_done_data = g_new (CallDoneData, 1);
      call_done_data->portal = g_object_ref (portal);
      call_done_data->callback = callback,
      call_done_data->data = data;
    }

  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Notification",
                          "AddNotification",
                          g_variant_new ("(s@a{sv})", id, notification),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          call_done_cb,
                          call_done_data);
}

/**
 * xdp_portal_add_notification_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the notification request.
 *
 * Returns the result as a boolean.
 *
 * Returns: `TRUE` if the notification was added
 */
gboolean
xdp_portal_add_notification_finish (XdpPortal     *portal,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_autoptr(GVariant) res = NULL;

  res = g_dbus_connection_call_finish (portal->bus, result, error);

  return !!res;
}

/**
 * xdp_portal_remove_notification:
 * @portal: a [class@Portal]
 * @id: the ID of an notification
 *
 * Withdraws a desktop notification.
 */
void
xdp_portal_remove_notification (XdpPortal  *portal,
                                const char *id)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Notification",
                          "RemoveNotification",
                          g_variant_new ("(s)", id),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}
