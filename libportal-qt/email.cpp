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

#include "email.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

#include <string.h>
#include <iostream>

namespace Xdp
{

void PortalPrivate::composeEmail(const Parent &parent, const QStringList &addresses, const QStringList &cc,
                                 const QStringList &bcc, const QString &subject, const QString &body,
                                 const QStringList &attachments, EmailFlags flags)
{
    int i = 0;
    char *addressesArray[addresses.size() + 1];
    char *ccArray[cc.size() + 1];
    char *bccArray[bcc.size() + 1];
    char *attachmentsArray[attachments.size() + 1];

    while (i < addresses.size()) {
        addressesArray[i] = new char[addresses.at(i).length() + 1];
        strcpy(addressesArray[i], addresses.at(i).toLatin1());
        addressesArray[i][addresses.at(i).length()] = '\0';
        i++;
    }
    addressesArray[i] = nullptr;

    i = 0;
    while (i < cc.size()) {
        ccArray[i] = new char[cc.at(i).length() + 1];
        strcpy(ccArray[i], cc.at(i).toLatin1());
        ccArray[i][cc.at(i).length()] = '\0';
        i++;
    }
    ccArray[i] = nullptr;

    i = 0;
    while (i < bcc.size()) {
        bccArray[i] = new char[bcc.at(i).length() + 1];
        strcpy(bccArray[i], bcc.at(i).toLatin1());
        bccArray[i][bcc.at(i).length()] = '\0';
        i++;
    }
    bccArray[i] = nullptr;

    i = 0;
    while (i < attachments.size()) {
        attachmentsArray[i] = new char[attachments.at(i).length() + 1];
        strcpy(attachmentsArray[i], attachments.at(i).toLatin1());
        attachmentsArray[i][attachments.at(i).length()] = '\0';
        i++;
    }
    attachmentsArray[i] = nullptr;

    xdp_portal_compose_email(m_xdpPortal, parent.d_ptr->m_xdpParent, addressesArray, ccArray, bccArray, const_cast<char*>(subject.toUtf8().constData()),
                             const_cast<char*>(body.toUtf8().constData()), attachmentsArray, static_cast<XdpEmailFlags>((int)flags), nullptr, composedEmail, this);

    i = 0;
    while (i < addresses.size()) {
        delete addressesArray[i];
        i++;
    }

    i = 0;
    while (i < cc.size()) {
        delete ccArray[i];
        i++;
    }

    i = 0;
    while (i < bcc.size()) {
        delete bccArray[i];
        i++;
    }

    i = 0;
    while (i < attachments.size()) {
        delete attachmentsArray[i];
        i++;
    }
}

void PortalPrivate::composedEmail(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_compose_email_finish(xdpPortal, result, &error);

    Response response(ret, error);
    portalPrivate->composeEmailResponse(response);
}

}
