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

#include <libportal/portal-helpers.h>
#include <libportal/session.h>
#include <stdint.h>

G_BEGIN_DECLS

#define XDP_TYPE_GLOBAL_SHORTCUTS_SESSION (xdp_global_shortcuts_session_get_type ())

XDP_PUBLIC
G_DECLARE_FINAL_TYPE (XdpGlobalShortcutsSession, xdp_global_shortcuts_session, XDP, GLOBAL_SHORTCUTS_SESSION, GObject)


XDP_PUBLIC
void        xdp_portal_create_global_shortcuts_session (XdpPortal            *portal,
                                                     XdpParent            *parent,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              data);

XDP_PUBLIC
XdpGlobalShortcutsSession * xdp_portal_create_global_shortucuts_session_finish (XdpPortal     *portal,
                                                                         GAsyncResult  *result,
                                                                         GError       **error);

XDP_PUBLIC
XdpSession  *xdp_global_shortcuts_session_get_session (XdpGlobalShortcutsSession *session);

XDP_PUBLIC
GList *     xdp_global_shortcuts_session_get_zones (XdpGlobalShortcutsSession *session);

XDP_PUBLIC
void        xdp_global_shortcuts_session_set_pointer_barriers (XdpGlobalShortcutsSession         *session,
                                                            GList                          *barriers,
                                                            GCancellable                   *cancellable,
                                                            GAsyncReadyCallback             callback,
                                                            gpointer                        data);

XDP_PUBLIC
GList *     xdp_global_shortcuts_session_set_pointer_barriers_finish (XdpGlobalShortcutsSession  *session,
                                                                   GAsyncResult            *result,
                                                                   GError                 **error);

XDP_PUBLIC
void        xdp_global_shortcuts_session_enable (XdpGlobalShortcutsSession *session);

XDP_PUBLIC
void        xdp_global_shortcuts_session_disable (XdpGlobalShortcutsSession *session);

XDP_PUBLIC
void        xdp_global_shortcuts_session_release_at (XdpGlobalShortcutsSession *session,
                                                  guint                   activation_id,
                                                  gdouble                 cursor_x_position,
                                                  gdouble                 cursor_y_position);

XDP_PUBLIC
void        xdp_global_shortcuts_session_release (XdpGlobalShortcutsSession *session,
                                               guint activation_id);

XDP_PUBLIC
int        xdp_global_shortcuts_session_connect_to_eis (XdpGlobalShortcutsSession  *session,
                                                     GError                 **error);

G_END_DECLS
