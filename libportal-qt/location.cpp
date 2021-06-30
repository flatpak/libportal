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

#include "location.h"
#include "portal.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

namespace Xdp
{

void PortalPrivate::locationMonitorStart(const Parent &parent, uint distanceThreshold, uint timeThreshold, LocationAccuracy accuracy, LocationMonitorFlags flags)
{
    xdp_portal_location_monitor_start(m_xdpPortal, parent.d_ptr->m_xdpParent, distanceThreshold, timeThreshold, static_cast<XdpLocationAccuracy>(accuracy),
                                      static_cast<XdpLocationMonitorFlags>((int)flags), nullptr /* cancellable */, locationMonitorStarted, this);
}

void PortalPrivate::locationMonitorStarted(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_location_monitor_start_finish(xdpPortal, result, &error);

    Response response(ret, error);

    portalPrivate->locationMonitorStartResponse(response);
}

void PortalPrivate::locationMonitorStop()
{
    xdp_portal_location_monitor_stop(m_xdpPortal);
}

void PortalPrivate::onLocationUpdated(XdpPortal *portal, double latitude, double longitude, double altitude, double accuracy, double speed, double heading, const char *description, qint64 timestamp_s, qint64 timestamp_ms,  gpointer data)
{
    Q_UNUSED(portal)

    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);

    portalPrivate->locationUpdated(latitude, longitude, altitude, accuracy, speed, heading, QString(description), timestamp_s, timestamp_ms);
}

}
