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
#include <QPrinter>

namespace Xdp
{

Q_GLOBAL_STATIC(PortalPrivate, globalPortal)

PortalPrivate::PortalPrivate()
    : m_xdpPortal(xdp_portal_new())
{
    g_signal_connect(m_xdpPortal, "session-state-changed", G_CALLBACK(onSessionStateChanged), this);
    g_signal_connect(m_xdpPortal, "location-updated", G_CALLBACK(onLocationUpdated), this);
    g_signal_connect(m_xdpPortal, "notification-action-invoked", G_CALLBACK(onNotificationActionInvoked), this);
}

PortalPrivate::~PortalPrivate()
{
    if (m_xdpPortal) {
        g_object_unref(m_xdpPortal);
    }
}

// Account portal
void getUserInformation(const Parent &parent, const QString &reason, UserInformationFlags flags)
{
    globalPortal->getUserInformation(parent, reason, flags);
}

// Background portal
void requestBackground(const Parent &parent, const QString &reason, const QStringList &commandline, BackgroundFlags flags)
{
    globalPortal->requestBackground(parent, reason, commandline, flags);
}

// Camera portal
void accessCamera(const Parent &parent, CameraFlags flags)
{
    globalPortal->accessCamera(parent, flags);
}

int openPipewireRemoteForCamera()
{
    return globalPortal->openPipewireRemoteForCamera();
}

// Email portal
void composeEmail(const Parent &parent, const QStringList &addresses, const QStringList &cc,
                       const QStringList &bcc, const QString &subject, const QString &body,
                       const QStringList &attachments, EmailFlags flags)
{
    globalPortal->composeEmail(parent, addresses, cc, bcc, subject, body, attachments, flags);
}

// FileChooser portal
void openFile(const Parent &parent, const QString &title, const FileChooserFilterList &filters, const FileChooserFilter &currentFilter,
                   const FileChooserChoices &choices, OpenFileFlags flags)
{
    globalPortal->openFile(parent, title, filters, currentFilter, choices, flags);
}

void saveFile(const Parent &parent, const QString &title, const QString &currentName, const QString &currentFolder, const QString &currentFile,
                   const FileChooserFilterList &filters, const FileChooserFilter &currentFilter, const FileChooserChoices &choices, SaveFileFlags flags)
{
    globalPortal->saveFile(parent, title, currentName, currentFolder, currentFile, filters, currentFilter, choices, flags);
}

void saveFiles(const Parent &parent, const QString &title, const QString &currentFolder, const QStringList &files,
                    const FileChooserChoices &choices, SaveFileFlags flags)
{
    globalPortal->saveFiles(parent, title, currentFolder, files, choices, flags);
}

// Inhibit portal
void sessionInhibit(const Parent &parent, const QString &reason, InhibitFlags flags)
{
    globalPortal->sessionInhibit(parent, reason, flags);
}

void sessionUninhibit(int id)
{
    globalPortal->sessionUninhibit(id);
}

void sessionMonitorStart(const Parent &parent, SessionMonitorFlags flags)
{
    globalPortal->sessionMonitorStart(parent, flags);
}

void sessionMonitorStop()
{
    globalPortal->sessionMonitorStop();
}

void sessionMonitorQueryEndResponse()
{
    globalPortal->sessionMonitorQueryEndResponse();
}

// Location portal
void locationMonitorStart(const Parent &parent, uint distanceThreshold, uint timeThreshold, LocationAccuracy accuracy, LocationMonitorFlags flags)
{
    globalPortal->locationMonitorStart(parent, distanceThreshold, timeThreshold, accuracy, flags);
}

void locationMonitorStop()
{
    globalPortal->locationMonitorStop();
}

// Notification portal
void addNotification(const QString& id, const Notification& notification, NotificationFlags flags)
{
    globalPortal->addNotification(id, notification, flags);
}

void removeNotification(const QString& id)
{
    globalPortal->removeNotification(id);
}

// OpenURI portal
void openUri(const Parent &parent, const QString &uri, OpenUriFlags flags)
{
    globalPortal->openUri(parent, uri, flags);
}

void openDirectory(const Parent &parent, const QString &uri, OpenUriFlags flags)
{
    globalPortal->openDirectory(parent, uri, flags);
}

// Print portal
void preparePrint(const Parent &parent, const QString &title, const QPrinter &printer, PrintFlags flags)
{
    globalPortal->preparePrint(parent, title, printer, flags);
}

void printFile(const Parent &parent, const QString &title, int token, const QString &file, PrintFlags flags)
{
    globalPortal->printFile(parent, title, token, file, flags);
}

bool isSandboxed()
{
    return QFileInfo::exists(QStringLiteral("/.flatpak-info")) || qEnvironmentVariableIsSet("SNAP");
}

Notifier *notifier()
{
    return globalPortal;
}

}

