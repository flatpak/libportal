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

#define XDP_TYPE_PORTAL (xdp_portal_get_type ())

G_DECLARE_FINAL_TYPE (XdpPortal, xdp_portal, XDP, PORTAL, GObject)

#ifndef XDP_PUBLIC
#define XDP_PUBLIC extern
#endif

XDP_PUBLIC
GType      xdp_portal_get_type               (void) G_GNUC_CONST;

XDP_PUBLIC
XdpPortal *xdp_portal_new                    (void);

/**
 * XdpParent:
 *
 * A struct that provides information about parent windows.
 * The members of this struct are private to libportal and should
 * not be accessed by applications.
 */
typedef struct _XdpParent XdpParent;

typedef void     (* XdpParentExported) (XdpParent         *parent,
                                        const char        *handle,
                                        gpointer           data);
typedef gboolean (* XdpParentExport)   (XdpParent         *parent,
                                        XdpParentExported  callback,
                                        gpointer           data);
typedef void     (* XdpParentUnexport) (XdpParent         *parent);

struct _XdpParent {
  /*< private >*/
  XdpParentExport export;
  XdpParentUnexport unexport;
  GObject *object;
  XdpParentExported callback;
  gpointer data;
};

static inline void xdp_parent_free (XdpParent *parent);

static inline void xdp_parent_free (XdpParent *parent)
{
  g_clear_object (&parent->data);
  g_free (parent);
}

G_END_DECLS
