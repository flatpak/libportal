/*
 * Copyright (C) 2025 Red Hat
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
#include <libportal/session.h>

G_BEGIN_DECLS

XDP_PUBLIC
void            xdp_session_request_clipboard           (XdpSession    *session);

XDP_PUBLIC
gboolean        xdp_session_is_clipboard_enabled        (XdpSession    *session);

XDP_PUBLIC
gboolean        xdp_session_is_selection_owned_by_session (XdpSession  *session);

XDP_PUBLIC
const char **   xdp_session_get_selection_mime_types    (XdpSession  *session);

XDP_PUBLIC
void            xdp_session_set_selection               (XdpSession    *session,
                                                         const char   **mime_types);

XDP_PUBLIC
int             xdp_session_selection_write             (XdpSession    *session,
                                                         unsigned int   serial);

XDP_PUBLIC
void            xdp_session_selection_write_done        (XdpSession    *session,
                                                         unsigned int   serial,
                                                         gboolean       success);

XDP_PUBLIC
int             xdp_session_selection_read              (XdpSession    *session,
                                                         const char    *mime_type);

G_END_DECLS
