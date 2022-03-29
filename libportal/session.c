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
 * xdp_session_close:
 * @session: an active [class@Session]
 *
 * Closes the session.
 */
void
xdp_session_close (XdpSession *session)
{
  g_return_if_fail (XDP_IS_SESSION (session));

  g_dbus_connection_call (session->portal->bus,
                          PORTAL_BUS_NAME,
                          session->id,
                          SESSION_INTERFACE,
                          "Close",
                          NULL,
                          NULL, 0, -1, NULL, NULL, NULL);

  _xdp_session_set_session_state (session, XDP_SESSION_CLOSED);
  _xdp_session_close (session);
}
