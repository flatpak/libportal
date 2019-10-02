/*
 * Copyright (C) 2019, Matthias Clasen
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

#include "portal.h"

struct _XdpLocation {
  GObject parent_instance;

  XdpPortal *portal;
  XdpSessionState state;
  char *id;

  guint signal_id;
  guint updated_id;
};

XdpLocation * _xdp_location_new (XdpPortal *portal,
                                 const char *id);

void         _xdp_location_set_session_state (XdpLocation *location,
                                              XdpSessionState state);
