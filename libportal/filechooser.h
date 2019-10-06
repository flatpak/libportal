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
void       xdp_portal_open_file                   (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *title,
                                                   gboolean              modal,
                                                   gboolean              multiple,
                                                   GVariant             *filters,
                                                   GVariant             *choices,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
GVariant *xdp_portal_open_file_finish             (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

XDP_PUBLIC
void       xdp_portal_save_file                   (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *title,
                                                   gboolean              modal,
                                                   const char           *current_name,
                                                   const char           *current_folder,
                                                   const char           *current_file,
                                                   GVariant             *filters,
                                                   GVariant             *choices,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
GVariant *xdp_portal_save_file_finish             (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

G_END_DECLS
