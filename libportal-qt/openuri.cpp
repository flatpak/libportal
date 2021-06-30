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

#include "portal.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

using namespace Xdp;

void PortalPrivate::openUri(const Parent &parent, const QString &uri, OpenUriFlags flags)
{
    xdp_portal_open_uri(m_xdpPortal, parent.d_ptr->m_xdpParent, uri.toStdString().c_str(), static_cast<XdpOpenUriFlags>((int)flags), nullptr, openedUri, this);
}

void PortalPrivate::openDirectory(const Parent &parent, const QString &uri, Xdp::OpenUriFlags flags)
{
    xdp_portal_open_directory(m_xdpPortal, parent.d_ptr->m_xdpParent, uri.toStdString().c_str(), static_cast<XdpOpenUriFlags>((int)flags), nullptr, openedDirectory, this);
}

void PortalPrivate::openedUri(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_open_uri_finish(xdpPortal, result, &error);

    Response response(ret, error ? QString(error->message) : QString());

    portalPrivate->openUriResponse(response);
}

void PortalPrivate::openedDirectory(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_open_directory_finish(xdpPortal, result, &error);

    Response response(ret, error ? QString(error->message) : QString());

    portalPrivate->openDirectoryResponse(response);
}
