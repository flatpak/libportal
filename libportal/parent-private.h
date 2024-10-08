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

#pragma once

#include "parent.h"

G_BEGIN_DECLS

typedef void     (* XdpParentExported) (XdpParent         *parent,
                                        const char        *handle,
                                        gpointer           data);
typedef gboolean (* XdpParentExport)   (XdpParent         *parent,
                                        XdpParentExported  callback,
                                        gpointer           data);
typedef void     (* XdpParentUnexport) (XdpParent         *parent);

struct _XdpParent {
  /*< private >*/
  XdpParentExport parent_export;
  XdpParentUnexport parent_unexport;
  GObject *object;
  XdpParentExported callback;
  char *exported_handle;
  gpointer data;
};

G_END_DECLS
