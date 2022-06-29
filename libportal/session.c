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

#include "session-private.h"
#include "portal-private.h"

/**
 * XdpSession
 *
 * A representation of long-lived screencast portal interactions.
 *
 * The XdpSession object is used to represent portal interactions with the
 * screencast or remote desktop portals that extend over multiple portal calls.
 *
 * To find out what kind of session an XdpSession object represents and whether
 * it is still active, you can use [method@Session.get_session_type] and
 * [method@Session.get_session_state].
 *
 * All sessions start in an initial state. They can be made active by calling
 * [method@Session.start], and ended by calling [method@Session.close].
 */
enum {
  CLOSED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (XdpSession, xdp_session, G_TYPE_OBJECT)

static void
xdp_session_finalize (GObject *object)
{
  XdpSession *session = XDP_SESSION (object);

  if (session->signal_id)
    g_dbus_connection_signal_unsubscribe (session->portal->bus, session->signal_id);

  g_clear_object (&session->portal);
  g_clear_pointer (&session->restore_token, g_free);
  g_clear_pointer (&session->id, g_free);
  g_clear_pointer (&session->streams, g_variant_unref);

  G_OBJECT_CLASS (xdp_session_parent_class)->finalize (object);
}

static void
xdp_session_class_init (XdpSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_session_finalize;

  /**
   * XdpSession::closed:
   *
   * Emitted when a session is closed externally.
   */
  signals[CLOSED] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
xdp_session_init (XdpSession *session)
{
}

static void
session_closed (GDBusConnection *bus,
                const char *sender_name,
                const char *object_path,
                const char *interface_name,
                const char *signal_name,
                GVariant *parameters,
                gpointer data)
{
  XdpSession *session = data;

  _xdp_session_set_session_state (session, XDP_SESSION_CLOSED);
}

XdpSession *
_xdp_session_new (XdpPortal *portal,
                  const char *id,
                  XdpSessionType type)
{
  XdpSession *session;

  session = g_object_new (XDP_TYPE_SESSION, NULL);
  session->portal = g_object_ref (portal);
  session->id = g_strdup (id);
  session->type = type;
  session->state = XDP_SESSION_INITIAL;

  session->signal_id = g_dbus_connection_signal_subscribe (portal->bus,
                                                           PORTAL_BUS_NAME,
                                                           SESSION_INTERFACE,
                                                           "Closed",
                                                           id,
                                                           NULL,
                                                           G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                           session_closed,
                                                           session,
                                                           NULL);
  return session;
}

/**
 * xdp_session_get_session_type:
 * @session: an [class@Session]
 *
 * Obtains information about the type of session that is represented
 * by @session.
 *
 * Returns: the type of @session
 */
XdpSessionType
xdp_session_get_session_type (XdpSession *session)
{
  g_return_val_if_fail (XDP_IS_SESSION (session), XDP_SESSION_SCREENCAST);

  return session->type;
}

/**
 * xdp_session_get_session_state:
 * @session: an [class@Session]
 *
 * Obtains information about the state of the session that is represented
 * by @session.
 *
 * Returns: the state of @session
 */
XdpSessionState
xdp_session_get_session_state (XdpSession *session)
{
  g_return_val_if_fail (XDP_IS_SESSION (session), XDP_SESSION_CLOSED);

  return session->state;
}

void
_xdp_session_set_session_state (XdpSession *session,
                                XdpSessionState state)
{
  session->state = state;

  if (state == XDP_SESSION_INITIAL && session->state != XDP_SESSION_INITIAL)
    {
      g_warning ("Can't move a session back to initial state");
      return;
    }
  if (session->state == XDP_SESSION_CLOSED && state != XDP_SESSION_CLOSED)
    {
      g_warning ("Can't move a session back from closed state");
      return;
    }

  if (state == XDP_SESSION_CLOSED)
    g_signal_emit (session, signals[CLOSED], 0);
}

/**
 * xdp_session_get_devices:
 * @session: a [class@Session]
 *
 * Obtains the devices that the user selected.
 *
 * Unless the session is active, this function returns `XDP_DEVICE_NONE`.
 *
 * Returns: the selected devices
 */
XdpDeviceType
xdp_session_get_devices (XdpSession *session)
{
  g_return_val_if_fail (XDP_IS_SESSION (session), XDP_DEVICE_NONE);

  if (session->state != XDP_SESSION_ACTIVE)
    return XDP_DEVICE_NONE;

  return session->devices;
}

void
_xdp_session_set_devices (XdpSession *session,
                          XdpDeviceType devices)
{
  session->devices = devices;
}

/**
 * xdp_session_get_streams:
 * @session: a [class@Session]
 *
 * Obtains the streams that the user selected.
 *
 * The information in the returned [struct@GLib.Variant] has the format
 * `a(ua{sv})`. Each item in the array is describing a stream. The first member
 * is the pipewire node ID, the second is a dictionary of stream properties,
 * including:
 *
 * - position, `(ii)`: a tuple consisting of the position `(x, y)` in the compositor
 *     coordinate space. Note that the position may not be equivalent to a
 *     position in a pixel coordinate space. Only available for monitor streams.
 * - size, `(ii)`: a tuple consisting of (width, height). The size represents the size
 *     of the stream as it is displayed in the compositor coordinate space.
 *     Note that this size may not be equivalent to a size in a pixel coordinate
 *     space. The size may differ from the size of the stream.
 *
 * Unless the session is active, this function returns `NULL`.
 *
 * Returns: the selected streams
 */
GVariant *
xdp_session_get_streams (XdpSession *session)
{
  g_return_val_if_fail (XDP_IS_SESSION (session), NULL);

  if (session->state != XDP_SESSION_ACTIVE)
    return NULL;

  return session->streams;
}

void
_xdp_session_set_streams (XdpSession *session,
                          GVariant *streams)
{
  if (session->streams)
    g_variant_unref (session->streams);
  session->streams = streams;
  if (session->streams)
    g_variant_ref (session->streams);
}
