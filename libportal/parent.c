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
#include "parent-private.h"


/**
 * XdpParent
 *
 * Parent window abstraction.
 *
 * The [struct@Parent] struct provides an abstract way to represent
 * a window, without introducing a dependency on a toolkit
 * library.
 *
 * An XdpParent implementation for GTK is included in the
 * `portal-gtk3.h` and `portal-gtk4.h` header files, in the form of inline functions.
 * To create a XdpParent for a GTK window, use
 * `xdp_parent_new_gtk()`.
 */
G_DEFINE_BOXED_TYPE (XdpParent, xdp_parent, xdp_parent_copy, xdp_parent_free)

/**
 * xdp_parent_copy:
 * @source: a [struct@Parent]
 *
 * Copies @source into a new [struct@Parent].
 *
 * Returns: (transfer full): an [struct@Parent] that is a copy of @source
 */
XdpParent *
xdp_parent_copy (XdpParent *source)
{
  XdpParent *parent;

  parent = g_new0 (XdpParent, 1);

  parent->parent_export = source->parent_export;
  parent->parent_unexport = source->parent_unexport;
  g_set_object (&parent->object, source->object);
  parent->data = source->data;

  return parent;
}

/**
 * xdp_parent_free:
 * @parent: an [struct@Parent]
 *
 * Frees @parent.
 */
void
xdp_parent_free (XdpParent *parent)
{
  g_clear_object (&parent->object);
  g_free (parent);
}
