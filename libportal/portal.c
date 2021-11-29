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

#include "portal-helpers.h"
#include "portal-private.h"
#include "portal-enums.h"

/**
 * XdpPortal
 *
 * Context for portal calls.
 *
 * The XdpPortal object provides the main context object
 * for the portal operations of libportal.
 *
 * Typically, an application will create a single XdpPortal
 * object with [ctor@Portal.new] and use it throughout its lifetime.
 */

enum {
  SPAWN_EXITED,
  SESSION_STATE_CHANGED,
  UPDATE_AVAILABLE,
  UPDATE_PROGRESS,
  LOCATION_UPDATED,
  NOTIFICATION_ACTION_INVOKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (XdpPortal, xdp_portal, G_TYPE_OBJECT)

static void
xdp_portal_finalize (GObject *object)
{
  XdpPortal *portal = XDP_PORTAL (object);

  /* inhibit */
  if (portal->inhibit_handles)
    g_hash_table_unref (portal->inhibit_handles);

  if (portal->state_changed_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->state_changed_signal);

  g_free (portal->session_monitor_handle);

  /* spawn */
  if (portal->spawn_exited_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->spawn_exited_signal);

  /* updates */
  if (portal->update_available_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->update_available_signal);
  if (portal->update_progress_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->update_progress_signal);
  g_free (portal->update_monitor_handle);

  /* location */
  if (portal->location_updated_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->location_updated_signal);
  g_free (portal->location_monitor_handle);

  /* notification */
  if (portal->action_invoked_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->action_invoked_signal);

  g_clear_object (&portal->bus);
  g_free (portal->sender);

  G_OBJECT_CLASS (xdp_portal_parent_class)->finalize (object);
}

static void
xdp_portal_class_init (XdpPortalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_portal_finalize;

  /**
   * XdpPortal::spawn-exited:
   * @portal: the [class@Portal] object
   * @pid: the pid of the process
   * @exit_status: the exit status of the process
   *
   * This signal is emitted when a process that was spawned
   * with [method@Portal.spawn] exits.
   */
  signals[SPAWN_EXITED] = g_signal_new ("spawn-exited",
                                        G_TYPE_FROM_CLASS (object_class),
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 2,
                                        G_TYPE_UINT,
                                        G_TYPE_UINT);

  /**
   * XdpPortal::session-state-changed:
   * @portal: the [class@Portal] object
   * @screensaver_active: whether the screensaver is active
   * @session_state: the current state of the login session
   *
   * This signal is emitted when session state monitoring is
   * enabled and the state of the login session changes or
   * the screensaver is activated or deactivated.
   */
  signals[SESSION_STATE_CHANGED] = g_signal_new ("session-state-changed",
                                                 G_TYPE_FROM_CLASS (object_class),
                                                 G_SIGNAL_RUN_FIRST,
                                                 0,
                                                 NULL, NULL,
                                                 NULL,
                                                 G_TYPE_NONE, 2,
                                                 G_TYPE_BOOLEAN,
                                                 XDP_TYPE_LOGIN_SESSION_STATE);

  /**
   * XdpPortal::update-available:
   * @portal: the [class@Portal] object
   * @running_commit: the commit that the sandbox is running with
   * @local_commit: the commit that is currently deployed. Restarting
   *     the app will use this commit
   * @remote_commit: the commit that is available as an update.
   *     Updating the app will deloy this commit
   *
   * This signal is emitted when updates monitoring is enabled
   * and a new update is available. It is only sent once with
   * the same information, but it can be sent many times if
   * new updates appear.
   */
  signals[UPDATE_AVAILABLE] = g_signal_new ("update-available",
                                            G_TYPE_FROM_CLASS (object_class),
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL, NULL,
                                            NULL,
                                            G_TYPE_NONE, 3,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);

  /**
   * XdpPortal::update-progress:
   * @portal: the [class@Portal] object
   * @n_ops: the number of operations that the update consists of
   * @op: the position of the currently active operation
   * @progress: the progress of the currently active operation, as
   *   a number between 0 and 100
   * @status: the overall status of the update
   * @error: the error name if the status is 'failed'
   * @error_message: the error message if the status is 'failed'
   *
   * This signal gets emitted to indicate progress of an
   * update installation. It is undefined exactly how often it
   * is sent, but it will be emitted at least once at the end with
   * a non-zero @status. For each successful operation in the
   * update, we're also guaranteed to send exactly one signal
   * with @progress 100.
   */
  signals[UPDATE_PROGRESS] = g_signal_new ("update-progress",
                                           G_TYPE_FROM_CLASS (object_class),
                                           G_SIGNAL_RUN_FIRST,
                                           0,
                                           NULL, NULL,
                                           NULL,
                                           G_TYPE_NONE, 6,
                                           G_TYPE_UINT,
                                           G_TYPE_UINT,
                                           G_TYPE_UINT,
                                           XDP_TYPE_UPDATE_STATUS,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);

 /**
   * XdpPortal::location-updated:
   * @portal: the [class@Portal]
   * @latitude: the latitude, in degrees
   * @longitude: the longitude, in degrees
   * @altitude: the altitude, in meters
   * @accuracy: the accuracy, in meters
   * @speed: the speed, in meters per second
   * @heading: the heading, in degrees
   * @description: the description
   * @timestamp_s: the timestamp seconds since the Unix epoch
   * @timestamp_ms: the microseconds fraction of the timestamp
   *
   * The ::location-updated signal is emitted when location
   * monitoring is enabled and the location changes.
   */
  signals[LOCATION_UPDATED] =
    g_signal_new ("location-updated",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 9,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_STRING,
                  G_TYPE_INT64,
                  G_TYPE_INT64);

  /**
   * XdpPortal::notification-action-invoked:
   * @portal: the [class@Portal]
   * @id: the notification ID
   * @action: the action name
   * @parameter: (nullable): the target parameter for the action
   *
   * The ::notification-action-invoked signal is emitted when
   * a non-exported action is activated on a notification.
   */
  signals[NOTIFICATION_ACTION_INVOKED] =
    g_signal_new ("notification-action-invoked",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_VARIANT);
}

static void
xdp_portal_init (XdpPortal *portal)
{
  int i;

  portal->bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  portal->sender = g_strdup (g_dbus_connection_get_unique_name (portal->bus) + 1);
  for (i = 0; portal->sender[i]; i++)
    if (portal->sender[i] == '.')
      portal->sender[i] = '_';
}

/**
 * xdp_portal_new:
 *
 * Creates a new [class@Portal] object.
 *
 * Returns: a newly created [class@Portal] object
 */
XdpPortal *
xdp_portal_new (void)
{
  return g_object_new (XDP_TYPE_PORTAL, NULL);
}
