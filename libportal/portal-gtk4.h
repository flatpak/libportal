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

#include <libportal/portal.h>
#include <gtk/gtk.h>

#if !(GTK_CHECK_VERSION(3,96,0) || GTK_CHECK_VERSION(4,0,0))
#error "To use libportal with GTK3, include portal-gtk3.h"
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

G_BEGIN_DECLS

static inline void _xdp_parent_exported_wayland (GdkToplevel *toplevel,
                                                 const char *handle,
                                                 gpointer data)

{
  XdpParent *parent = data;
  g_autofree char *handle_str = g_strdup_printf ("wayland:%s", handle);
  parent->callback (parent, handle_str, parent->data);
}

static inline gboolean _xdp_parent_export_gtk (XdpParent *parent,
                                               XdpParentExported callback,
                                               gpointer data)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (parent->object))))
    {
      GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (parent->object));
      guint32 xid = (guint32) gdk_x11_surface_get_xid (surface);
      g_autofree char *handle = g_strdup_printf ("x11:%x", xid);
      callback (parent, handle, data);
      return TRUE;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (parent->object))))
    {
      GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (parent->object));
      parent->callback = callback;
      parent->data = data;
      return gdk_wayland_toplevel_export_handle (GDK_TOPLEVEL (surface),
                                                 _xdp_parent_exported_wayland,
                                                 parent,
                                                 NULL);
    }
#endif
  g_warning ("Couldn't export handle, unsupported windowing system");
  return FALSE;
}

static inline void _xdp_parent_unexport_gtk (XdpParent *parent)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (parent->object))))
    {
      GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (parent->object));
      gdk_wayland_toplevel_unexport_handle (GDK_TOPLEVEL (surface));
    }
#endif
}

static inline XdpParent *xdp_parent_new_gtk (GtkWindow *window)
{
  XdpParent *parent = g_new0 (XdpParent, 1);
  parent->parent_export = _xdp_parent_export_gtk;
  parent->parent_unexport = _xdp_parent_unexport_gtk;
  parent->object = (GObject *) g_object_ref (window);
  return parent;
}

G_END_DECLS
