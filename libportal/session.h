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

#pragma once

#include <libportal/types.h>

G_BEGIN_DECLS

#define XDP_TYPE_SESSION (xdp_session_get_type ())

XDP_PUBLIC
G_DECLARE_FINAL_TYPE (XdpSession, xdp_session, XDP, SESSION, GObject)

/**
 * XdpSessionType:
 * @XDP_SESSION_SCREENCAST: a screencast session.
 * @XDP_SESSION_REMOTE_DESKTOP: a remote desktop session.
 *
 * The type of a session.
 */
typedef enum {
  XDP_SESSION_SCREENCAST,
  XDP_SESSION_REMOTE_DESKTOP,
} XdpSessionType;

/**
 * xdp_portal_create_session:
 *
 * Create a new session. This session can later be used to combine
 * session-based actions from multiple portals into one so they count
 * as one logical entity.
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.create_session_finish] to get the results.
 *
 * This call requires org.freedesktop.portal.Session version 2 or
 * higher. If this version is not available and for backwards compatibility
 * this call returns a special XdpSession object that does not
 * represent a Session on DBus yet. The DBus session will be created
 * automatically later when either of
 * xdp_remote_desktop_create_session() or xdp_screencast_create_session()
 * is called.
 */
XDP_PUBLIC
void            xdp_portal_create_session (XdpPortal            *portal,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              data);

XDP_PUBLIC
XdpSession *    xdp_portal_create_session_finish (XdpPortal            *portal,
                                                  GAsyncResult         *result,
                                                  GError              **error);

XDP_PUBLIC
void            xdp_session_close             (XdpSession *session);

G_DEPRECATED_FOR(xdp_session_has_session_type)
XDP_PUBLIC
XdpSessionType  xdp_session_get_session_type  (XdpSession *session);

/**
 * xdp_session_has_session_type:
 *
 * Returns: true if the given session has the given type.
 */
XDP_PUBLIC
gboolean        xdp_session_has_session_type  (XdpSession *session, XdpSessionType type);

G_END_DECLS
