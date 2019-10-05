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
#include "portal-enums.h"

/**
 * SECTION:portal
 * @title: XdpPortal
 * @short_description: context for portal calls
 *
 * The XdpPortal object provides the main context object
 * for the portal operations of libportal.
 *
 * Typically, an application will create a single XdpPortal
 * object with xdp_portal_new() and use it throughout its lifetime.
 */

enum {
  SPAWN_EXITED,
  SESSION_STATE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (XdpPortal, xdp_portal, G_TYPE_OBJECT)

static void
xdp_portal_finalize (GObject *object)
{
  XdpPortal *portal = XDP_PORTAL (object);

  if (portal->spawn_exited_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->spawn_exited_signal);
  if (portal->state_changed_signal)
    g_dbus_connection_signal_unsubscribe (portal->bus, portal->state_changed_signal);

  g_free (portal->session_monitor_handle);

  g_clear_object (&portal->bus);
  g_free (portal->sender);

  if (portal->inhibit_handles)
    g_hash_table_unref (portal->inhibit_handles);

  G_OBJECT_CLASS (xdp_portal_parent_class)->finalize (object);
}

static void
xdp_portal_class_init (XdpPortalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_portal_finalize;

  /**
   * XdpPortal::spawn-exited:
   * @portal: the #XdpPortal object
   * @pid: the pid of the process
   * @exit_status: the exit status of the process
   *
   * This signal is emitted when a process that was spawned
   * with xdp_portal_spawn() exits.
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
   * @portal: the #XdpPortal object
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
 * Creates a new #XdpPortal object.
 *
 * Returns: a newly created #XdpPortal object
 */
XdpPortal *
xdp_portal_new (void)
{
  return g_object_new (XDP_TYPE_PORTAL, NULL);
}
