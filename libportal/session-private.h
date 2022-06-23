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

#include <libportal/remote.h>

struct _XdpSessionClass
{
  GObjectClass parent_class;

  void (*close) (XdpSession *session);

  gpointer padding[24];
};

XdpSession * _xdp_session_new (XdpPortal *portal,
                               const char *id,
                               XdpSessionType type);

const char * _xdp_session_get_id (XdpSession *session);

XdpPortal  * _xdp_session_get_portal (XdpSession *session);

void         _xdp_session_set_session_state (XdpSession *session,
                                             XdpSessionState state);

void         _xdp_session_set_devices (XdpSession *session,
                                       XdpDeviceType devices);

XdpDeviceType _xdp_session_get_devices (XdpSession *session);

void         _xdp_session_set_streams (XdpSession *session,
                                       GVariant   *streams);

void         _xdp_session_set_persist_mode (XdpSession *session,
                                            XdpPersistMode persist_mode);

void         _xdp_session_set_restore_token (XdpSession *session,
                                             char *restore_token);

const char * _xdp_session_get_restore_token (XdpSession *session);

XdpPersistMode _xdp_session_get_persist_mode (XdpSession *session);
