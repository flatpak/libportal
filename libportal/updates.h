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

#include <portal-helpers.h>

G_BEGIN_DECLS

/**
 * XdpUpdateStatus:
 * @XDP_UPDATE_STATUS_RUNNING: Update in progress
 * @XDP_UPDATE_STATUS_EMPTY: No update to install
 * @XDP_UPDATE_STATUS_DONE: Update finished successfully
 * @XDP_UPDATE_STATUS_FAILED: Update failed
 *
 * The values of this enum are returned in the
 * #XdpPortal::update-progress signal to indicate
 * the current progress of an update installation.
 */
typedef enum {
  XDP_UPDATE_STATUS_RUNNING,
  XDP_UPDATE_STATUS_EMPTY,
  XDP_UPDATE_STATUS_DONE,
  XDP_UPDATE_STATUS_FAILED
} XdpUpdateStatus;

XDP_PUBLIC
void       xdp_portal_update_monitor_start        (XdpPortal            *portal,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
gboolean   xdp_portal_update_monitor_start_finish (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

XDP_PUBLIC
void       xdp_portal_update_monitor_stop         (XdpPortal            *portal);

XDP_PUBLIC
void       xdp_portal_update_install              (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
gboolean   xdp_portal_update_install_finish       (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

G_END_DECLS
