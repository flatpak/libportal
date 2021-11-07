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

#include "config.h"

#include "parent-private.h"
#include "utils-private.h"

XdpParent *
_xdp_parent_copy (XdpParent *source)
{
  XdpParent *parent;

  parent = g_new0 (XdpParent, 1);

  parent->parent_export = source->parent_export;
  parent->parent_unexport = source->parent_unexport;
  g_set_object (&parent->object, source->object);
  parent->data = source->data;

  return parent;
}

void
_xdp_parent_free (XdpParent *parent)
{
  g_clear_object (&parent->object);
  g_free (parent);
}
