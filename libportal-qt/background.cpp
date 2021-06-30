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

#include "background.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

using namespace Xdp;

void PortalPrivate::requestBackground(const Parent &parent, const QString &reason, const QStringList &commandline, BackgroundFlags flags)
{
    g_autoptr(GPtrArray) ptrArray = g_ptr_array_new();

    for (const QString &cmdPart : commandline) {
        g_ptr_array_add(ptrArray, static_cast<gpointer>(g_strdup(cmdPart.toStdString().c_str())));
    }

    xdp_portal_request_background(m_xdpPortal, parent.d_ptr->m_xdpParent, const_cast<char*>(reason.toStdString().c_str()), ptrArray, static_cast<XdpBackgroundFlags>((int)flags), nullptr, requestedBackground, this);
}

void PortalPrivate::requestedBackground(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_request_background_finish(xdpPortal, result, &error);

    Response response(ret, error ? QString(error->message) : QString());

    portalPrivate->requestBackgroundResponse(response);
}
