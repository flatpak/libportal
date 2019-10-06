/*
 * Copyright (C) 2018, Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "notification.h"
#include "portal-private.h"

/**
 * SECTION:notification
 * @title: Notification
 * @short_description: send notifications
 * 
 * These functions let applications send desktop notifications.
 *
 * The underlying portal is org.freedesktop.portal.Notification.
 */

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
  g_autoptr(GVariant) info = NULL;
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

/**
 * xdp_portal_add_notification:
 * @portal: a #XdpPortal
 * @id: unique ID for the notification
 * @notification: a #GVariant dictionary with the content of the notification
 *
 * Sends a desktop notification.
 *
 * The following keys may be present in @notification:
 * - title `s`: a user-visible string to display as title
 * - body `s`: a user-visible string to display as body
 * - icon `v`: a serialized icon (in the format produced by g_icon_serialize())
 * - priority `s`: "low", "normal", "high" or "urgent"
 * - default-action `s`: name of an action that
 *     will be activated when the user clicks on the notification
 * - default-action-target `v`: target parameter to send along when
 *     activating the default action.
 * - buttons `aa{sv}`: array of serialized buttons
 *
 * Each serialized button is a dictionary with the following supported keys:
 * - label `s`: user-visible lable for the button. Mandatory
 * - action `s`: name of an action that will be activated when
 *     the user clicks on the button. Mandatory
 * - target `v`: target parameter to send along when activating
 *     the button
 *
 * Actions with a prefix of "app." are assumed to be exported by the
 * application and will be activated via the org.freedesktop.Application
 * interface, others are activated by emitting the
 * #XdpPortal::notification-action-invoked signal.
 *
 * It is the callers responsibility to ensure that the ID is unique
 * among all notifications.
 *
 * To withdraw a notification, use xdp_portal_remove_notification().
 */
void
xdp_portal_add_notification (XdpPortal  *portal,
                             const char *id,
                             GVariant   *notification)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  ensure_action_invoked_connection (portal);

  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Notification",
                          "AddNotification",
                          g_variant_new ("(s@a{sv})", id, notification),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}

/**
 * xdp_portal_remove_notification:
 * @portal: a #XdpPortal
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
