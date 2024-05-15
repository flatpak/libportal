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
  g_autoptr(GVariant) platform_data = NULL;

  if (g_str_equal (signal_name, "ActionInvoked2"))
    {
      g_variant_get (parameters, "(&s&s@a{sv}@av)",
                     &id, &action, &platform_data, &parameter);
    }
  else
    {
      GVariantBuilder n;

      g_variant_get (parameters, "(&s&s@av)", &id, &action, &parameter);

      g_variant_builder_init (&n, G_VARIANT_TYPE_VARDICT);
      platform_data = g_variant_ref_sink (g_variant_builder_end (&n));
    }

  g_signal_emit_by_name (portal, "notification-action-invoked",
                         id, action, platform_data, parameter);
}

static void
ensure_action_invoked_connection (XdpPortal *portal)
{
  const char *signal_name;

  if (portal->notification_interface_version >= 2)
    signal_name = "ActionInvoked2";
  else
    signal_name = "ActionInvoked";

  if (portal->action_invoked_signal == 0)
    portal->action_invoked_signal =
       g_dbus_connection_signal_subscribe (portal->bus,
                                           PORTAL_BUS_NAME,
                                           "org.freedesktop.portal.Notification",
                                           signal_name,
                                           PORTAL_OBJECT_PATH,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                           action_invoked,
                                           portal,
                                           NULL);
}

static void
call_done (GObject *source,
           GAsyncResult *result,
           gpointer data)
{
  g_autoptr(GVariant) res = NULL;
  g_autoptr(GTask) task = data;
  GError *error = NULL;
  XdpPortal *portal;

  portal = g_task_get_source_object (task);
  res = g_dbus_connection_call_finish (portal->bus, result, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, !!res);
}

static void
add_notification_internal (XdpPortal *portal,
                           GVariant *parameters,
                           GCancellable *cancellable,
                           GTask *task)
{
  GAsyncReadyCallback call_done_cb = task ? call_done : NULL;

  ensure_action_invoked_connection (portal);

  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Notification",
                          "AddNotification",
                          parameters,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          call_done_cb,
                          task);
}

static void
get_version_returned (GObject *object,
                      GAsyncResult *result,
                      gpointer data)
{
  g_autoptr(GVariant) version_variant = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GTask) task = data;
  XdpPortal *portal = g_task_get_source_object (task);
  GError *error = NULL;
  GVariant *notification_params;
  GCancellable *cancellable;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    {
      g_task_return_error (task, error);
      return;
    }

  g_variant_get_child (ret, 0, "v", &version_variant);
  portal->notification_interface_version = g_variant_get_uint32 (version_variant);

  g_debug ("Notification version: %u", portal->notification_interface_version);

  notification_params = g_task_get_task_data (task);
  cancellable = g_task_get_cancellable (task);

  add_notification_internal (portal, notification_params, cancellable,
                             g_steal_pointer (&task));
}

static void
get_notification_interface_version (XdpPortal *portal,
                                    GTask *task)
{
  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          g_variant_new ("(ss)", "org.freedesktop.portal.Notification", "version"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          g_task_get_cancellable (task),
                          get_version_returned,
                          task);
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
  GTask *task = NULL;
  GVariant *parameters;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_NOTIFICATION_FLAG_NONE);

  parameters = g_variant_new ("(s@a{sv})", id, notification);

  if (callback || !portal->notification_interface_version)
    {
      task = g_task_new (portal, cancellable, callback, data);
      g_task_set_task_data (task, g_variant_ref_sink (parameters),
                            (GDestroyNotify) g_variant_unref);
    }

  if (!portal->notification_interface_version)
    {
      get_notification_interface_version (portal, task);
    }
  else
    {
      add_notification_internal (portal, parameters, cancellable, task);
    }
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
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
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
