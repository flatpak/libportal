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

#ifndef LIBPORTALQT_PORTAL_P_H
#define LIBPORTALQT_PORTAL_P_H

#include "portal.h"

#undef signals
#include "portal-qt5.h"
#define signals Q_SIGNALS

class QPrinter;

namespace Xdp
{

class PortalPrivate : public Xdp::Notifier
{
    Q_OBJECT
public:
    PortalPrivate();
    virtual ~PortalPrivate();

    // Account portal
    void getUserInformation(const Parent &parent, const QString &reason, UserInformationFlags flags);
    static void gotUserInformation(GObject *object, GAsyncResult *result, gpointer data);

    // Background portal
    void requestBackground(const Parent &parent, const QString &reason, const QStringList &commandline, BackgroundFlags flags);
    static void requestedBackground(GObject *object, GAsyncResult *result, gpointer data);

    // Camera portal
    void accessCamera(const Parent &parent, CameraFlags flags);
    static void accessedCamera(GObject *object, GAsyncResult *result, gpointer data);

    int openPipewireRemoteForCamera();

    // Email
    void composeEmail(const Parent &parent, const QStringList &addresses, const QStringList &cc,
                      const QStringList &bcc, const QString &subject, const QString &body,
                      const QStringList &attachments, EmailFlags flags);
    static void composedEmail(GObject *object, GAsyncResult *result, gpointer data);

    // FileChooser portal
    void openFile(const Parent &parent, const QString &title, const FileChooserFilterList &filters, const FileChooserFilter &currentFilter,
                  const FileChooserChoices &choices, OpenFileFlags flags);
    static void openedFile(GObject *object, GAsyncResult *result, gpointer data);

    void saveFile(const Parent &parent, const QString &title, const QString &currentName, const QString &currentFolder, const QString &currentFile,
                  const FileChooserFilterList &filters, const FileChooserFilter &currentFilter, const FileChooserChoices &choices, SaveFileFlags flags);
    static void savedFile(GObject *object, GAsyncResult *result, gpointer data);

    void saveFiles(const Parent &parent, const QString &title, const QString &currentFolder, const QStringList &files,
                   const FileChooserChoices &choices, SaveFileFlags flags);
    static void savedFiles(GObject *object, GAsyncResult *result, gpointer data);

    static GVariant *filterToVariant(const FileChooserFilter &filter);
    static GVariant *filterListToVariant(const FileChooserFilterList &filters);
    static GVariant *choiceToVariant(const FileChooserChoice &choice);
    static GVariant *choicesToVariant(const FileChooserChoices &choices);
    static GVariant *filesToVariant(const QStringList &files);

    // Inhibit portal
    void sessionInhibit(const Parent &parent, const QString &reason, InhibitFlags flags);
    static void sessionInhibited(GObject *object, GAsyncResult *result, gpointer data);
    void sessionUninhibit(int id);
    void sessionMonitorStart(const Parent &parent, SessionMonitorFlags flags);
    static void sessionMonitorStarted(GObject *object, GAsyncResult *result, gpointer data);
    void sessionMonitorStop();
    void sessionMonitorQueryEndResponse();
    static void onSessionStateChanged(XdpPortal *portal, gboolean screenSaverActive, XdpLoginSessionState currentState, gpointer data);

    // Location portal
    void locationMonitorStart(const Parent &parent, uint distanceThreshold, uint timeThreshold, LocationAccuracy accuracy,
                              LocationMonitorFlags flags);
    static void locationMonitorStarted(GObject *object, GAsyncResult *result, gpointer data);
    void locationMonitorStop();
    static void onLocationUpdated(XdpPortal *portal, double latitude, double longitude, double altitude, double accuracy, double speed, double heading, const char *description, qint64 timestamp_s, qint64 timestamp_ms,  gpointer data);

    // Notification portal
    void addNotification(const QString &id, const Notification &notification, NotificationFlags flags);
    static void notificationAdded(GObject *object, GAsyncResult *result, gpointer data);
    void removeNotification(const QString &id);
    static void onNotificationActionInvoked(XdpPortal *portal, const char *id, const char *action, GVariant *parameter, gpointer data);

    static GVariant *buttonsToVariant(const NotificationButtons &buttons);
    static GVariant *notificationToVariant(const Notification &notification);

    // OpenURI portal
    void openUri(const Parent &parent, const QString &uri, OpenUriFlags flags);
    static void openedUri(GObject *object, GAsyncResult *result, gpointer data);

    // Print portal
    void preparePrint(const Parent &parent, const QString &title, const QPrinter &printer, PrintFlags flags);
    static void preparedPrint(GObject *object, GAsyncResult *result, gpointer data);
    void printFile(const Parent &parent, const QString &title, int token, const QString &file, PrintFlags flags);
    static void printedFile(GObject *object, GAsyncResult *result, gpointer data);

    void openDirectory(const Parent &parent, const QString &uri, OpenUriFlags flags);
    static void openedDirectory(GObject *object, GAsyncResult *result, gpointer data);
private:
    XdpPortal *m_xdpPortal = nullptr;
};
}

#endif // LIBPORTALQT_PORTAL_P_H
