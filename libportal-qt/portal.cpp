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
#include "portal_p.h"

#include <QFileInfo>

Q_GLOBAL_STATIC(Xdp::PortalPrivate, globalPortal)

Xdp::PortalPrivate::PortalPrivate()
    : m_xdpPortal(xdp_portal_new())
{
    g_signal_connect(m_xdpPortal, "session-state-changed", G_CALLBACK(onSessionStateChanged), this);
    g_signal_connect(m_xdpPortal, "location-updated", G_CALLBACK(onLocationUpdated), this);
}

Xdp::PortalPrivate::~PortalPrivate()
{
    if (m_xdpPortal) {
        g_object_unref(m_xdpPortal);
    }
}

// Account portal
void Xdp::getUserInformation(const Parent &parent, const QString &reason, UserInformationFlags flags)
{
    globalPortal->getUserInformation(parent, reason, flags);
}

// Background portal
void Xdp::requestBackground(const Parent &parent, const QString &reason, const QStringList &commandline, BackgroundFlags flags)
{
    globalPortal->requestBackground(parent, reason, commandline, flags);
}

// Camera portal
void Xdp::accessCamera(const Parent &parent, CameraFlags flags)
{
    globalPortal->accessCamera(parent, flags);
}

int Xdp::openPipewireRemoteForCamera()
{
    return globalPortal->openPipewireRemoteForCamera();
}

// Email portal
void Xdp::composeEmail(const Parent &parent, const QStringList &addresses, const QStringList &cc,
                       const QStringList &bcc, const QString &subject, const QString &body,
                       const QStringList &attachments, EmailFlags flags)
{
    globalPortal->composeEmail(parent, addresses, cc, bcc, subject, body, attachments, flags);
}

// FileChooser portal
void Xdp::openFile(const Parent &parent, const QString &title, const FileChooserFilterList &filters, const FileChooserFilter &currentFilter,
                   const FileChooserChoices &choices, OpenFileFlags flags)
{
    globalPortal->openFile(parent, title, filters, currentFilter, choices, flags);
}

void Xdp::saveFile(const Parent &parent, const QString &title, const QString &currentName, const QString &currentFolder, const QString &currentFile,
                   const FileChooserFilterList &filters, const FileChooserFilter &currentFilter, const FileChooserChoices &choices, SaveFileFlags flags)
{
    globalPortal->saveFile(parent, title, currentName, currentFolder, currentFile, filters, currentFilter, choices, flags);
}

void Xdp::saveFiles(const Parent &parent, const QString &title, const QString &currentFolder, const QStringList &files,
                    const FileChooserChoices &choices, SaveFileFlags flags)
{
    globalPortal->saveFiles(parent, title, currentFolder, files, choices, flags);
}

// Inhibit portal
void Xdp::sessionInhibit(const Xdp::Parent &parent, const QString &reason, Xdp::InhibitFlags flags)
{
    globalPortal->sessionInhibit(parent, reason, flags);
}

void Xdp::sessionUninhibit(int id)
{
    globalPortal->sessionUninhibit(id);
}

void Xdp::sessionMonitorStart(const Xdp::Parent &parent, Xdp::SessionMonitorFlags flags)
{
    globalPortal->sessionMonitorStart(parent, flags);
}

void Xdp::sessionMonitorStop()
{
    globalPortal->sessionMonitorStop();
}

void Xdp::sessionMonitorQueryEndResponse()
{
    globalPortal->sessionMonitorQueryEndResponse();
}

// Location portal
void Xdp::locationMonitorStart(const Xdp::Parent &parent, uint distanceThreshold, uint timeThreshold, Xdp::LocationAccuracy accuracy, Xdp::LocationMonitorFlags flags)
{
    globalPortal->locationMonitorStart(parent, distanceThreshold, timeThreshold, accuracy, flags);
}

void Xdp::locationMonitorStop()
{
    globalPortal->locationMonitorStop();
}

// OpenURI portal
void Xdp::openUri(const Parent &parent, const QString &uri, OpenUriFlags flags)
{
    globalPortal->openUri(parent, uri, flags);
}

void Xdp::openDirectory(const Parent &parent, const QString &uri, OpenUriFlags flags)
{
    globalPortal->openDirectory(parent, uri, flags);
}

bool Xdp::isSandboxed()
{
    return QFileInfo::exists(QStringLiteral("/.flatpak-info")) || qEnvironmentVariableIsSet("SNAP");
}

Xdp::Notifier *Xdp::notifier()
{
    return globalPortal;
}

