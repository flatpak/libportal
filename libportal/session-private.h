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

#pragma once

#include <libportal/remote.h>

struct _XdpSession {
  GObject parent_instance;

  XdpPortal *portal;
  char *id;
  XdpSessionType type;
  XdpSessionState state;
  XdpDeviceType devices;
  GVariant *streams;

  guint signal_id;
};

XdpSession * _xdp_session_new (XdpPortal *portal,
                               const char *id,
                               XdpSessionType type);

void         _xdp_session_set_session_state (XdpSession *session,
                                             XdpSessionState state);

void         _xdp_session_set_devices (XdpSession *session,
                                       XdpDeviceType devices);

void         _xdp_session_set_streams (XdpSession *session,
                                       GVariant   *streams);
