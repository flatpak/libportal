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
 * - default-action `s`: name of an action that is exported by the application.
 *     this action will be activated when the user clicks on the notification
 * - default-action-target `v`: target parameter to send along when activating
 *     the default action.
 * - buttons `aa{sv}`: array of serialized buttons
 *
 * Each serialized button is a dictionary with the following supported keys:
 * - label `s`: user-visible lable for the button. Mandatory
 * - action `s`: name of an action that is exported by the application. The action
 *     will be activated when the user clicks on the button. Mandatory
 * - target `v`: target parameter to send along when activating the button
 *
 * It is the callers responsibility to ensure that the ID is unique among
 * all notifications.
 *
 * To withdraw a notification, use xdp_portal_remove_notification().
 */
void
xdp_portal_add_notification (XdpPortal  *portal,
                             const char *id,
                             GVariant   *notification)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

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
