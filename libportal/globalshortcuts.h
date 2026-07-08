/*
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

G_BEGIN_DECLS

#define XDP_TYPE_GLOBAL_SHORTCUTS_SESSION (xdp_global_shortcuts_session_get_type ())

XDP_PUBLIC
G_DECLARE_FINAL_TYPE (XdpGlobalShortcutsSession,
                      xdp_global_shortcuts_session,
                      XDP,
                      GLOBAL_SHORTCUTS_SESSION,
                      GObject)

#define XDP_TYPE_GLOBAL_SHORTCUT (xdp_global_shortcut_get_type ())
#define XDP_TYPE_GLOBAL_SHORTCUT_ASSIGNED (xdp_global_shortcut_assigned_get_type ())

typedef struct _XdpGlobalShortcut XdpGlobalShortcut;
typedef struct _XdpGlobalShortcutAssigned XdpGlobalShortcutAssigned;

XDP_PUBLIC
GType                        xdp_global_shortcut_get_type                            (void) G_GNUC_CONST;

XDP_PUBLIC
XdpGlobalShortcut           *xdp_global_shortcut_new                                 (const char                      *shortcut_id,
                                                                                      const char                      *description,
                                                                                      const char                      *preferred_trigger);

XDP_PUBLIC
XdpGlobalShortcut           *xdp_global_shortcut_copy                                (const XdpGlobalShortcut         *shortcut);

XDP_PUBLIC
void                         xdp_global_shortcut_free                                (XdpGlobalShortcut               *shortcut);

XDP_PUBLIC
const char                  *xdp_global_shortcut_get_shortcut_id                     (const XdpGlobalShortcut         *shortcut);

XDP_PUBLIC
const char                  *xdp_global_shortcut_get_description                     (const XdpGlobalShortcut         *shortcut);

XDP_PUBLIC
const char                  *xdp_global_shortcut_get_preferred_trigger               (const XdpGlobalShortcut         *shortcut);

XDP_PUBLIC
void                         xdp_global_shortcut_set_description                     (XdpGlobalShortcut               *shortcut,
                                                                                      const char                      *description);

XDP_PUBLIC
void                         xdp_global_shortcut_set_preferred_trigger               (XdpGlobalShortcut               *shortcut,
                                                                                      const char                      *preferred_trigger);

XDP_PUBLIC
GType                        xdp_global_shortcut_assigned_get_type                   (void) G_GNUC_CONST;

XDP_PUBLIC
XdpGlobalShortcutAssigned   *xdp_global_shortcut_assigned_copy                       (const XdpGlobalShortcutAssigned *shortcut);

XDP_PUBLIC
void                         xdp_global_shortcut_assigned_free                       (XdpGlobalShortcutAssigned       *shortcut);

XDP_PUBLIC
const char                  *xdp_global_shortcut_assigned_get_shortcut_id            (const XdpGlobalShortcutAssigned *shortcut);

XDP_PUBLIC
const char                  *xdp_global_shortcut_assigned_get_description            (const XdpGlobalShortcutAssigned *shortcut);

XDP_PUBLIC
const char                  *xdp_global_shortcut_assigned_get_trigger_description    (const XdpGlobalShortcutAssigned *shortcut);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (XdpGlobalShortcut, xdp_global_shortcut_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (XdpGlobalShortcutAssigned, xdp_global_shortcut_assigned_free)

XDP_PUBLIC
void                         xdp_portal_create_global_shortcuts_session              (XdpPortal                       *portal,
                                                                                      GCancellable                    *cancellable,
                                                                                      GAsyncReadyCallback              callback,
                                                                                      gpointer                         data);

XDP_PUBLIC
XdpGlobalShortcutsSession   *xdp_portal_create_global_shortcuts_session_finish       (XdpPortal                       *portal,
                                                                                      GAsyncResult                    *result,
                                                                                      GError                         **error);

XDP_PUBLIC
XdpSession                  *xdp_global_shortcuts_session_get_session                (XdpGlobalShortcutsSession       *session);

XDP_PUBLIC
void                         xdp_global_shortcuts_session_close                      (XdpGlobalShortcutsSession       *session);

XDP_PUBLIC
void                         xdp_global_shortcuts_session_bind_shortcuts             (XdpGlobalShortcutsSession       *session,
                                                                                      GPtrArray                       *shortcuts,
                                                                                      const char                      *parent_window,
                                                                                      GCancellable                    *cancellable,
                                                                                      GAsyncReadyCallback              callback,
                                                                                      gpointer                         data);

XDP_PUBLIC
GPtrArray                   *xdp_global_shortcuts_session_bind_shortcuts_finish      (XdpGlobalShortcutsSession       *session,
                                                                                      GAsyncResult                    *result,
                                                                                      GError                         **error);

XDP_PUBLIC
void                         xdp_global_shortcuts_session_list_shortcuts             (XdpGlobalShortcutsSession       *session,
                                                                                      GCancellable                    *cancellable,
                                                                                      GAsyncReadyCallback              callback,
                                                                                      gpointer                         data);

XDP_PUBLIC
GPtrArray                   *xdp_global_shortcuts_session_list_shortcuts_finish      (XdpGlobalShortcutsSession       *session,
                                                                                      GAsyncResult                    *result,
                                                                                      GError                         **error);

XDP_PUBLIC
void                         xdp_global_shortcuts_session_configure_shortcuts        (XdpGlobalShortcutsSession       *session,
                                                                                      const char                      *parent_window,
                                                                                      const char                      *activation_token,
                                                                                      GCancellable                    *cancellable,
                                                                                      GAsyncReadyCallback              callback,
                                                                                      gpointer                         data);

XDP_PUBLIC
gboolean                     xdp_global_shortcuts_session_configure_shortcuts_finish (XdpGlobalShortcutsSession       *session,
                                                                                      GAsyncResult                    *result,
                                                                                      GError                         **error);

G_END_DECLS
