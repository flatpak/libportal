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
 * XdpSessionState:
 * @XDP_SESSION_INITIAL: the session has not been started.
 * @XDP_SESSION_ACTIVE: the session is active.
 * @XDP_SESSION_CLOSED: the session is no longer active.
 *
 * The state of a session.
 */
typedef enum {
  XDP_SESSION_INITIAL,
  XDP_SESSION_ACTIVE,
  XDP_SESSION_CLOSED
} XdpSessionState;

XDP_PUBLIC
void            xdp_session_close             (XdpSession *session);

XDP_PUBLIC
XdpSessionType  xdp_session_get_session_type  (XdpSession *session);

XDP_PUBLIC
XdpSessionState xdp_session_get_session_state (XdpSession *session);

G_END_DECLS
