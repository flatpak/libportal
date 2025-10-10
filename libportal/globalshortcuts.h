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


struct XdpGlobalShortcut {
    // Shortcut ID, Owned by caller
    char *name;
    // Shortcut description, Owned by caller
    char *description;
    // Shortcut suggested trigger, nullable, Owned by caller
    char *preferred_trigger;
};

struct XdpGlobalShortcutAssigned {
    // Shortcut ID, Owned
    char *name;
    // Human-readable trigger description, Owned
    char *trigger_description;
};


XDP_PUBLIC
void        xdp_portal_create_global_shortcuts_session (XdpPortal            *portal,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              data);

XDP_PUBLIC
XdpGlobalShortcutsSession * xdp_portal_create_global_shortcuts_session_finish (XdpPortal     *portal,
                                                                         GAsyncResult  *result,
                                                                         GError       **error);

XDP_PUBLIC
XdpSession  *xdp_global_shortcuts_session_get_session (XdpGlobalShortcutsSession *session);

XDP_PUBLIC
void xdp_global_shortcuts_session_close (XdpGlobalShortcutsSession *session);

XDP_PUBLIC
void        xdp_global_shortcuts_session_bind_shortcuts(XdpGlobalShortcutsSession         *session,
                                                 // Contains XdpGlobalShortcut elements
                                                            GArray                          *shortcuts, char *parent_window,
                                                            GCancellable                   *cancellable,
                                                            GAsyncReadyCallback             callback,
                                                            gpointer                        data);

XDP_PUBLIC
GArray *     xdp_global_shortcuts_session_bind_shortcuts_finish (XdpGlobalShortcutsSession  *session,
                                                                   GAsyncResult            *result,
                                                                   GError                 **error);

XDP_PUBLIC
void        xdp_global_shortcuts_session_release (XdpGlobalShortcutsSession *session);


G_END_DECLS
