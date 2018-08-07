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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XDP_TYPE_PORTAL (xdp_portal_get_type ())

G_DECLARE_FINAL_TYPE (XdpPortal, xdp_portal, XDP, PORTAL, GObject)

GType      xdp_portal_get_type               (void) G_GNUC_CONST;

XdpPortal *xdp_portal_new                    (void);

/* Screenshot */

void       xdp_portal_take_screenshot        (XdpPortal           *portal,
                                              GtkWindow           *parent,
                                              gboolean             modal,
                                              gboolean             interactive,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             data);
GdkPixbuf *xdp_portal_take_screenshot_finish (XdpPortal           *portal,
                                              GAsyncResult        *result,
                                              GError             **error);


/* Notification */

void       xdp_portal_add_notification    (XdpPortal  *portal,
                                           const char *id,
                                           GVariant   *notification);
void       xdp_portal_remove_notification (XdpPortal  *portal,
                                           const char *id);

/* Email */

void       xdp_portal_compose_email        (XdpPortal            *portal,
                                            GtkWindow            *parent,
                                            const char           *address,
                                            const char           *subject,
                                            const char           *body,
                                            const char *const    *attachments,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              data);

gboolean   xdp_portal_compose_email_finish (XdpPortal            *portal,
                                            GAsyncResult         *result,
                                            GError              **error);

G_END_DECLS
