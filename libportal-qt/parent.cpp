/*
 * Copyright (C) 2020-2021, Jan Grulich
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

#include "parent.h"
#include "parent_p.h"

using namespace Xdp;

ParentPrivate::ParentPrivate(QWindow *window, Parent *q)
    : m_xdpParent(xdp_parent_new_qt(window))
    , q_ptr(q)
{
}

ParentPrivate::~ParentPrivate()
{
    if (m_xdpParent) {
        xdp_parent_free(m_xdpParent);
    }
}

Parent::Parent(QWindow *window, QObject *parent)
    : QObject(parent)
    , d_ptr(new ParentPrivate(window, this))
{
}

Parent::~Parent()
{
}

