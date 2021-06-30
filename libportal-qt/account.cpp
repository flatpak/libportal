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

#include "account.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

namespace Xdp
{

void Xdp::PortalPrivate::getUserInformation(const Parent &parent, const QString &reason, UserInformationFlags flags)
{
    xdp_portal_get_user_information(m_xdpPortal, parent.d_ptr->m_xdpParent, reason.toUtf8().constData(), static_cast<XdpUserInformationFlags>((int)flags), nullptr, gotUserInformation, this);
}

void Xdp::PortalPrivate::gotUserInformation(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;
    g_autofree gchar *id = nullptr;
    g_autofree gchar *name = nullptr;
    g_autofree gchar *image = nullptr;

    g_autoptr(GVariant) ret = xdp_portal_get_user_information_finish(xdpPortal, result, &error);

    if (ret) {
        QVariantMap responseData;
        if (g_variant_lookup(ret, "id", "s", &id)) {
            responseData.insert(QStringLiteral("id"), id);
        }

        if (g_variant_lookup(ret, "name", "s", &name)) {
            responseData.insert(QStringLiteral("name"), name);
        }

        if (g_variant_lookup(ret, "image", "s", &image)) {
            responseData.insert(QStringLiteral("image"), image);
        }

        Response response(true, error, responseData);
        Q_EMIT portalPrivate->getUserInformationResponse(response);
    } else {
        Response response(false, error);
        Q_EMIT portalPrivate->getUserInformationResponse(response);
    }
}

}
