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
#include "portal-test-qt.h"
#include "ui_portal-test-qt.h"

#include <QStringLiteral>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>

PortalTestQt::PortalTestQt(QWidget *parent, Qt::WindowFlags f)
    : QMainWindow(parent, f)
    , m_mainWindow(new Ui_PortalTestQt)
{
    m_mainWindow->setupUi(this);

    // Account portal
    connect(m_mainWindow->getUserInformationButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::getUserInformation(xdpParent, QStringLiteral("Testing libportal"), Xdp::UserInformationFlag::None);
        connect(Xdp::notifier(), &Xdp::Notifier::getUserInformationResponse, this, &PortalTestQt::onUserInformationReceived);
    });

    // Background portal
    connect(m_mainWindow->requestBackgroundButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        QStringList commandline = {QStringLiteral("/usr/bin/portal-test-qt"), QStringLiteral("some parameter"), QStringLiteral("--some-option")};
        Xdp::requestBackground(xdpParent, QStringLiteral("Testing libportal"), commandline, Xdp::BackgroundFlag::Autostart);
        connect(Xdp::notifier(), &Xdp::Notifier::requestBackgroundResponse, this, [=] (const Xdp::Response &response) {
            if (response.isSuccess()) {
                QMessageBox::information(this, QStringLiteral("Background Portal"), QStringLiteral("This application will successfully autostart"));
            }
        });
    });

    // Camera portal
    connect(m_mainWindow->accessCameraButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::accessCamera(xdpParent, Xdp::CameraFlag::None);
    });

    // Email portal
    connect(m_mainWindow->composeEmailButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        const QStringList addresses = {QStringLiteral("foo@bar.org"), QStringLiteral("bar@foo.org")};
        const QStringList cc = {QStringLiteral("foo@bar.org"), QStringLiteral("bar@foo.org")};
        const QStringList bcc = {QStringLiteral("foo@bar.org"), QStringLiteral("bar@foo.org")};
        const QString subject = QStringLiteral("Hello");
        const QString body = QStringLiteral("This is a portal test");

        Xdp::composeEmail(xdpParent, addresses, cc, bcc, subject, body, QStringList(), Xdp::EmailFlag::None);
    });

    // FileChooser portal
    connect(m_mainWindow->openFileButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::FileChooserFilterRule rule(Xdp::FileChooserFilterRule::Type::Mimetype, QStringLiteral("image/jpeg"));
        Xdp::FileChooserFilterRule rule2;
        rule2.setType(Xdp::FileChooserFilterRule::Type::Pattern);
        rule2.setRule(QStringLiteral("*.png"));
        Xdp::FileChooserFilter filter(QStringLiteral("Images"), {rule});
        filter.addRule(rule2);
        Xdp::FileChooserChoice choice(QStringLiteral("choice-id"), QStringLiteral("choice-label"),
                                      QMap<QString, QString>{{QStringLiteral("option1-id"), QStringLiteral("option1-value")}}, QStringLiteral("option1-id"));
        choice.addOption(QStringLiteral("option2-id"), QStringLiteral("option2-value"));

        Xdp::openFile(xdpParent, QStringLiteral("Portal Test Qt"), {filter}, filter, {choice}, Xdp::OpenFileFlag::Multiple);
        connect(Xdp::notifier(), &Xdp::Notifier::openFileResponse, this, &PortalTestQt::onFileOpened);
    });
    connect(m_mainWindow->saveFileButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::saveFile(xdpParent, QStringLiteral("Portal Test Qt "), QStringLiteral("name.txt"), QStringLiteral("/tmp"), QStringLiteral("name_old.txt"),
                      {}, Xdp::FileChooserFilter(), {}, Xdp::SaveFileFlag::None);
    });
    connect(m_mainWindow->saveFilesButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::saveFiles(xdpParent, QStringLiteral("Portal Test Qt "), QStringLiteral("/tmp"), QStringList{QStringLiteral("foo.txt"), QStringLiteral("bar.txt")}, {}, Xdp::SaveFileFlag::None);
    });

    // Inhbit portal
    connect(m_mainWindow->inhibitButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::sessionInhibit(xdpParent, QStringLiteral("Portal-test: testing inhibit portal"), Xdp::InhibitFlag::Suspend);
        connect(Xdp::notifier(), &Xdp::Notifier::sessionInhibitResponse, this, &PortalTestQt::onSessionInhibited);
    });
    connect(m_mainWindow->uninhibitButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::sessionUninhibit(m_inhibitorId);
    });

    // Location portal
    connect(m_mainWindow->startLocationMonitorButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::locationMonitorStart(xdpParent, 5, 5, Xdp::LocationAccuracy::Exact, Xdp::LocationMonitorFlag::None);
        connect(Xdp::notifier(), &Xdp::Notifier::locationMonitorStartResponse, this, [=] (const Xdp::Response &response) {
            if (response.isSuccess()) {
                QMessageBox::information(this, QStringLiteral("Location monitor"), QStringLiteral("Location monitor successfully started"));
                connect(Xdp::notifier(), &Xdp::Notifier::locationUpdated, this, [=] (double latitude, double longitude, double altitude, double accuracy, double speed,
                                                                                     double heading, QString description, qint64 timestamp_s, qint64 timestamp_ms) {
                    QMessageBox::information(this, QStringLiteral("Location updated"),
                                             QStringLiteral("Latitude: %1 | Longitude %2 | Altitude %3 | Accuracy %4 | Description %5 | Timestamp_s %6").arg(latitude).arg(longitude).arg(altitude).arg(accuracy).arg(description).arg(timestamp_s));
                });
            }
        });
    });
    connect(m_mainWindow->stopLocationMonitorButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::locationMonitorStop();
    });

    // OpenURI portal
    connect(m_mainWindow->openLinkButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::openUri(xdpParent, QStringLiteral("https://github.com/flatpak/libportal"), Xdp::OpenUriFlag::None);
    });
    connect(m_mainWindow->openDirectoryButton, &QPushButton::clicked, [=] (bool clicked) {
        Xdp::Parent xdpParent(windowHandle());
        Xdp::openDirectory(xdpParent, QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first()).toDisplayString(), Xdp::OpenUriFlag::None);
    });
}

PortalTestQt::~PortalTestQt()
{
    delete m_mainWindow;
}

void PortalTestQt::onUserInformationReceived(const Xdp::Response &response)
{
    if (response.isSuccess()) {
        QString id = response.result().contains(QStringLiteral("id")) ? response.result().value(QStringLiteral("id")).toString() : QString();
        QString name = response.result().contains(QStringLiteral("name")) ? response.result().value(QStringLiteral("name")).toString() : QString();
        QString image = response.result().contains(QStringLiteral("image")) ? response.result().value(QStringLiteral("image")).toString() : QString();

        QMessageBox::information(this, QStringLiteral("User Information"), QStringLiteral("User ID: %1 | User Name: %2 | User Picture: %3").arg(id).arg(name).arg(image));
    }
}

void PortalTestQt::onFileOpened(const Xdp::Response &response)
{
    if (response.isSuccess()) {
        QStringList uris = response.result().value(QStringLiteral("uris")).toStringList();
        if (!uris.isEmpty()) {
            Xdp::Parent xdpParent(windowHandle());
            Xdp::openUri(xdpParent, uris.at(0), Xdp::OpenUriFlag::Ask);
        }
    }
}

void PortalTestQt::onSessionInhibited(const Xdp::Response &response)
{
    if (response.isSuccess()) {
        m_inhibitorId = response.result().value(QStringLiteral("id")).toInt();
    }
}
