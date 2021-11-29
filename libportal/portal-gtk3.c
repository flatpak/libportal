/*
 * Copyright (C) 2021, Georges Basile Stavracas Neto
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

#include "config.h"
#include "portal-gtk3.h"

#include "parent-private.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

static void
_xdp_parent_exported_wayland (GdkWindow  *window,
                              const char *handle,
                              gpointer    data)

{
  XdpParent *parent = data;
  g_autofree char *handle_str = g_strdup_printf ("wayland:%s", handle);
  parent->callback (parent, handle_str, parent->data);
}

static gboolean
_xdp_parent_export_gtk (XdpParent         *parent,
                        XdpParentExported  callback,
                        gpointer           data)
{
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (parent->object))))
    {
      GdkWindow *w = gtk_widget_get_window (GTK_WIDGET (parent->object));
      guint32 xid = (guint32) gdk_x11_window_get_xid (w);
      g_autofree char *handle = g_strdup_printf ("x11:%x", xid);
      callback (parent, handle, data);
      return TRUE;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (parent->object))))
    {
      GdkWindow *w = gtk_widget_get_window (GTK_WIDGET (parent->object));
      parent->callback = callback;
      parent->data = data;
      return gdk_wayland_window_export_handle (w, _xdp_parent_exported_wayland, parent, NULL);
    }
#endif
  g_warning ("Couldn't export handle, unsupported windowing system");
  return FALSE;
}

static void
_xdp_parent_unexport_gtk (XdpParent *parent)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (parent->object))))
    {
      GdkWindow *w = gtk_widget_get_window (GTK_WIDGET (parent->object));
      gdk_wayland_window_unexport_handle (w);
    }
#endif
}

/**
 * xdp_parent_new_gtk:
 * @window: a [class@Gtk.Window]
 *
 * Creates a new [struct@Parent] from @window.
 *
 * Returns: (transfer full): a [struct@Parent]
 */
XdpParent *
xdp_parent_new_gtk (GtkWindow *window)
{
  XdpParent *parent = g_new0 (XdpParent, 1);
  parent->parent_export = _xdp_parent_export_gtk;
  parent->parent_unexport = _xdp_parent_unexport_gtk;
  parent->object = (GObject *) g_object_ref (window);
  return parent;
}
