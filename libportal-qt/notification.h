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

#ifndef LIBPORTALQT_NOTIFICATION_H
#define LIBPORTALQT_NOTIFICATION_H

#include "libportalqt_export.h"

#include <QFlags>
#include <QString>
#include <QPixmap>
#include <QSharedPointer>

namespace Xdp
{

enum class NotificationFlag
{
    None = 0x0
};

class NotificationButtonPrivate;
class NotificationPrivate;

class LIBPORTALQT_EXPORT NotificationButton
{
public:
    NotificationButton(const QString &label, const QString &action);
    NotificationButton(const NotificationButton &other);

    ~NotificationButton();

    void operator=(const NotificationButton &other);

    bool isValid() const;

    QString label() const;
    void setLabel(const QString &label);

    QString action() const;
    void setAction(const QString &action);

    QVariant target() const;
    void setTarget(const QVariant &target);
private:
    Q_DECLARE_PRIVATE(NotificationButton)

    const QScopedPointer<NotificationButtonPrivate> d_ptr;
};

typedef QList<NotificationButton> NotificationButtons;


class LIBPORTALQT_EXPORT Notification
{
public:
    explicit Notification();
    Notification(const QString &title, const QString &body);
    Notification(const Notification &other);
    ~Notification();

    void operator=(const Notification &other);

    bool isValid() const;

    QString title() const;
    void setTitle(const QString &title);

    QString body() const;
    void setBody(const QString &body);

    QString icon() const;
    void setIcon(const QString &iconName);

    QPixmap pixmap() const;
    void setPixmap(const QPixmap &pixmap);

    QString priority() const;
    void setPriority(const QString &priority);

    QString defaultAction() const;
    void setDefaultAction(const QString &defaultAction);

    QVariant defaultTarget() const;
    void setDefaultActionTarget(const QVariant &target);

    NotificationButtons buttons() const;
    void addButton(const NotificationButton &button);
private:

    Q_DECLARE_PRIVATE(Notification)

    const QScopedPointer<NotificationPrivate> d_ptr;
};

Q_DECLARE_FLAGS(NotificationFlags, NotificationFlag)
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Xdp::NotificationFlags)

#endif // LIBPORTALQT_NOTIFICATION_H
