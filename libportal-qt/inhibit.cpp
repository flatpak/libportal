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

#include "inhibit.h"
#include "portal.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

namespace Xdp
{

void PortalPrivate::sessionInhibit(const Parent &parent, const QString &reason, InhibitFlags flags)
{
    xdp_portal_session_inhibit(m_xdpPortal, parent.d_ptr->m_xdpParent, reason.toStdString().c_str(),
                               static_cast<XdpInhibitFlags>((int)flags), nullptr /*cancellable*/, sessionInhibited, this);
}

void PortalPrivate::sessionInhibited(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    int ret = xdp_portal_session_inhibit_finish(xdpPortal, result, &error);

    QVariantMap responseData;
    responseData.insert(QStringLiteral("id"), ret);
    Response response(ret, error ? QString(error->message) : QString(), responseData);

    portalPrivate->sessionInhibitResponse(response);
}

void PortalPrivate::sessionUninhibit(int id)
{
    xdp_portal_session_uninhibit(m_xdpPortal, id);
}

void PortalPrivate::sessionMonitorStart(const Parent &parent, SessionMonitorFlags flags)
{
    xdp_portal_session_monitor_start(m_xdpPortal, parent.d_ptr->m_xdpParent, static_cast<XdpSessionMonitorFlags>((int)flags),
                                     nullptr /*cancellable*/, sessionMonitorStarted, this);
}

void PortalPrivate::sessionMonitorStarted(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_session_monitor_start_finish(xdpPortal, result, &error);

    Response response(ret, error ? QString(error->message) : QString());

    portalPrivate->sessionMonitorStartResponse(response);
}

void PortalPrivate::sessionMonitorStop()
{
    xdp_portal_session_monitor_stop(m_xdpPortal);
}

void PortalPrivate::sessionMonitorQueryEndResponse()
{
    xdp_portal_session_monitor_query_end_response(m_xdpPortal);
}

void PortalPrivate::onSessionStateChanged(XdpPortal *portal, gboolean screenSaverActive, XdpLoginSessionState currentState, gpointer data)
{
    Q_UNUSED(portal)

    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);

    portalPrivate->sessionStateChanged(screenSaverActive, static_cast<LoginSessionState>(currentState));
}

}
