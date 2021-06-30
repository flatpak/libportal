/*
 * Copyright (C) 2020, Jan Grulich
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

#include <QGuiApplication>
#include <QWindow>

static inline gboolean _xdp_parent_export_qt (XdpParent *parent,
                                              XdpParentExported callback,
                                              gpointer data)
{
  if (QGuiApplication::platformName() == QStringLiteral("xcb"))
    {
      QWindow *w = (QWindow *) parent->data;
      if (w) {
        guint32 xid = (guint32) w->winId ();
        g_autofree char *handle = g_strdup_printf ("x11:%x", xid);
        callback (parent, handle, data);
        return TRUE;
      }
    }
  else
    {
      /* TODO: QtWayland doesn't support xdg-foreign protocol yet
       * Upstream bugs: https://bugreports.qt.io/browse/QTBUG-73801
       *                https://bugreports.qt.io/browse/QTBUG-76983
       */
      g_warning ("QtWayland doesn't support xdg-foreign protocol yet");
      g_autofree char *handle = g_strdup ("");
      callback (parent, handle, data);
      return TRUE;
    }

  g_warning ("Couldn't export handle, unsupported windowing system");
  return FALSE;
}

static inline void _xdp_parent_unexport_qt (XdpParent *parent)
{
}

static inline XdpParent *xdp_parent_new_qt (QWindow *window);

static inline XdpParent *xdp_parent_new_qt (QWindow *window)
{
  XdpParent *parent = g_new0 (XdpParent, 1);
  parent->parent_export = _xdp_parent_export_qt;
  parent->parent_unexport = _xdp_parent_unexport_qt;
  parent->data = (gpointer) window;
  return parent;
}
