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

#include <gio/gio.h>

G_BEGIN_DECLS

#ifndef __GTK_DOC_IGNORE__
#ifndef XDP_PUBLIC
#define XDP_PUBLIC extern
#endif
#endif /* __GTK_DOC_IGNORE__ */

#define XDP_TYPE_PORTAL (xdp_portal_get_type ())

G_DECLARE_FINAL_TYPE (XdpPortal, xdp_portal, XDP, PORTAL, GObject)

XDP_PUBLIC
GType      xdp_portal_get_type               (void) G_GNUC_CONST;

XDP_PUBLIC
XdpPortal *xdp_portal_new                    (void);

G_END_DECLS
