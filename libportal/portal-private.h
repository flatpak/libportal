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

#include "portal.h"
#include "dbusgenerated.h"

struct _XdpPortal {
  GObject parent_instance;

  _xdpScreenshot *screenshot; 
  _xdpNotification *notification;
  _xdpEmail *email;
  _xdpAccount *account;
};

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define REQUEST_BUS_NAME "org.freedesktop.portal.Request"
#define REQUEST_INTERFACE "org.freedesktop.portal.Request"

