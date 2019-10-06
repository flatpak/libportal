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

#include <gio/gio.h>

G_BEGIN_DECLS

XDP_PUBLIC
void      xdp_portal_request_background         (XdpPortal           *portal,
                                                 XdpParent           *parent,
                                                 GPtrArray           *commandline,
                                                 char                *reason,
                                                 gboolean             autostart,
                                                 gboolean             dbus_activatable,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data);

XDP_PUBLIC
gboolean   xdp_portal_request_background_finish (XdpPortal     *portal,
                                                 GAsyncResult  *result,
                                                 GError       **error);


G_END_DECLS
