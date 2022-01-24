/*
 * Copyright (C) 2022, Matthew Leeds
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

XDP_PUBLIC
void      xdp_portal_prepare_install_launcher        (XdpPortal           *portal,
                                                      XdpParent           *parent,
                                                      const char          *name,
                                                      const char          *icon_file,
                                                      const char          *url,
                                                      gboolean             editable_name,
                                                      gboolean             editable_icon,
                                                      GCancellable        *cancellable,
                                                      GAsyncReadyCallback  callback,
                                                      gpointer             data);

XDP_PUBLIC
GVariant *xdp_portal_prepare_install_launcher_finish (XdpPortal            *portal,
                                                      GAsyncResult         *result,
                                                      GError              **error);

XDP_PUBLIC
char     *xdp_portal_request_launcher_install_token  (XdpPortal   *portal,
                                                      const char  *name,
                                                      const char  *icon_file,
                                                      GError     **error);

XDP_PUBLIC
int       xdp_portal_install_launcher                (XdpPortal   *portal,
                                                      const char  *token,
                                                      const char  *desktop_file_id,
                                                      const char  *desktop_entry,
                                                      GError     **error);

XDP_PUBLIC
int       xdp_portal_uninstall_launcher              (XdpPortal   *portal,
                                                      const char  *desktop_file_id,
                                                      GError     **error);

XDP_PUBLIC
char     *xdp_portal_get_desktop_entry               (XdpPortal   *portal,
                                                      const char  *desktop_file_id,
                                                      GError     **error);

G_END_DECLS
