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

#include "notification.h"
#include "portal.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

#include <QBuffer>

namespace Xdp
{

class NotificationButtonPrivate
{
public:
    NotificationButtonPrivate(const QString &label, const QString &action);
    NotificationButtonPrivate(const NotificationButtonPrivate &other);
    ~NotificationButtonPrivate() { };

    QString m_label;
    QString m_action;
    QVariant m_target;
};

NotificationButtonPrivate::NotificationButtonPrivate(const QString &label, const QString &action)
    : m_label(label)
    , m_action(action)
{
}

NotificationButtonPrivate::NotificationButtonPrivate(const NotificationButtonPrivate &other)
    : NotificationButtonPrivate(other.m_label, other.m_action)
{
    m_target = other.m_target;
}

NotificationButton::NotificationButton(const QString &label, const QString &action)
    : d_ptr(new NotificationButtonPrivate(label, action))
{
}

NotificationButton::NotificationButton(const NotificationButton &other)
    : NotificationButton(other.label(), other.action())
{
}

NotificationButton::~NotificationButton()
{
}

void NotificationButton::operator=(const NotificationButton &other)
{
    setLabel(other.label());
    setAction(other.action());
    setTarget(other.target());
}

bool NotificationButton::isValid() const
{
    Q_D(const NotificationButton);
    return !d->m_label.isEmpty() && !d->m_action.isEmpty();
}

QString NotificationButton::label() const
{
    Q_D(const NotificationButton);
    return d->m_label;
}

void NotificationButton::setLabel(const QString &label)
{
    Q_D(NotificationButton);
    d->m_label = label;
}

QString NotificationButton::action() const
{
    Q_D(const NotificationButton);
    return d->m_action;
}

void NotificationButton::setAction(const QString &action)
{
    Q_D(NotificationButton);
    d->m_action = action;
}

QVariant NotificationButton::target() const
{
    Q_D(const NotificationButton);
    return d->m_target;
}

void NotificationButton::setTarget(const QVariant &target)
{
    Q_D(NotificationButton);
    d->m_target = target;
}

class NotificationPrivate
{
public:
    explicit NotificationPrivate();
    NotificationPrivate(const QString &title, const QString &body);
    NotificationPrivate(const NotificationPrivate &other);
    ~NotificationPrivate() { };

    QString m_title;
    QString m_body;
    QString m_icon;
    QPixmap m_pixmap;
    QString m_priority;
    QString m_defaultAction;
    QVariant m_defaultTarget;
    NotificationButtons m_buttons;
};

NotificationPrivate::NotificationPrivate()
{
}

NotificationPrivate::NotificationPrivate(const QString &title, const QString &body)
    : m_title(title)
    , m_body(body)
{
}

NotificationPrivate::NotificationPrivate(const NotificationPrivate &other)
    : NotificationPrivate(other.m_title, other.m_body)
{
    m_icon = other.m_icon;
    m_pixmap = other.m_pixmap;
    m_priority = other.m_priority;
    m_defaultAction = other.m_defaultAction;
    m_defaultTarget = other.m_defaultTarget;
    m_buttons = other.m_buttons;
}

Notification::Notification()
    : d_ptr(new NotificationPrivate())
{
}

Notification::Notification(const QString &label, const QString &action)
    : d_ptr(new NotificationPrivate(label, action))
{
}

Notification::Notification(const Notification &other)
    : Notification(other.title(), other.body())
{
}

Notification::~Notification()
{
}

void Notification::operator=(const Notification &other)
{
    setTitle(other.title());
    setBody(other.body());
    setIcon(other.icon());
    setPixmap(other.pixmap());
    setPriority(other.priority());
    setDefaultAction(other.defaultAction());
    setDefaultActionTarget(other.defaultTarget());
    for (const NotificationButton &button : other.buttons()) {
        addButton(button);
    }
}

bool Notification::isValid() const
{
    Q_D(const Notification);
    return !d->m_title.isEmpty();
}

QString Notification::title() const
{
    Q_D(const Notification);
    return d->m_title;
}

void Notification::setTitle(const QString &title)
{
    Q_D(Notification);
    d->m_title = title;
}

QString Notification::body() const
{
    Q_D(const Notification);
    return d->m_body;
}

void Notification::setBody(const QString &body)
{
    Q_D(Notification);
    d->m_body = body;
}

QString Notification::icon() const
{
    Q_D(const Notification);
    return d->m_icon;
}

void Notification::setIcon(const QString &icon)
{
    Q_D(Notification);
    d->m_icon = icon;
}

QPixmap Notification::pixmap() const
{
    Q_D(const Notification);
    return d->m_pixmap;
}

void Notification::setPixmap(const QPixmap &pixmap)
{
    Q_D(Notification);
    d->m_pixmap = pixmap;
}

QString Notification::priority() const
{
    Q_D(const Notification);
    return d->m_priority;
}

void Notification::setPriority(const QString &priority)
{
    Q_D(Notification);
    d->m_priority = priority;
}

QString Notification::defaultAction() const
{
    Q_D(const Notification);
    return d->m_defaultAction;
}

void Notification::setDefaultAction(const QString &defaultAction)
{
    Q_D(Notification);
    d->m_defaultAction = defaultAction;
}

QVariant Notification::defaultTarget() const
{
    Q_D(const Notification);
    return d->m_defaultTarget;
}

void Notification::setDefaultActionTarget(const QVariant &defaultTarget)
{
    Q_D(Notification);
    d->m_defaultTarget = defaultTarget;
}

NotificationButtons Notification::buttons() const
{
    Q_D(const Notification);
    return d->m_buttons;
}

void Notification::addButton(const NotificationButton &button)
{
    Q_D(Notification);
    d->m_buttons << button;
}

static GVariant* convertVariant(const QVariant &variant)
{
    switch (variant.type()) {
    case QVariant::Bool:
        return g_variant_new_boolean(variant.toBool());
    case QVariant::ByteArray:
        return g_variant_new_bytestring(variant.toByteArray().data());
    case QVariant::Double:
        return g_variant_new_double(variant.toFloat());
    case QVariant::Int:
        return g_variant_new_int32(variant.toInt());
    case QVariant::LongLong:
        return g_variant_new_int64(variant.toLongLong());
    case QVariant::String:
        return g_variant_new_string(variant.toString().toUtf8().constData());
    case QVariant::UInt:
        return g_variant_new_uint32(variant.toUInt());
    case QVariant::ULongLong:
        return g_variant_new_uint64(variant.toULongLong());
    default:
        return nullptr;
    }
}

static QVariant convertVariant(GVariant *variant)
{
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN)) {
        return QVariant::fromValue<bool>(g_variant_get_boolean(variant));
    } else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BYTESTRING)) {
        return QVariant::fromValue<QByteArray>(g_variant_get_bytestring(variant));
    } else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_DOUBLE)) {
        return QVariant::fromValue<float>(g_variant_get_double(variant));
    } else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT32)) {
        return QVariant::fromValue<int>(g_variant_get_int32(variant));
    } else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT64)) {
        return QVariant::fromValue<long>(g_variant_get_int64(variant));
    } else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING)) {
        return QVariant::fromValue<QString>(g_variant_get_string(variant, nullptr));
    } else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT32)) {
        return QVariant::fromValue<uint>(g_variant_get_uint32(variant));
    } else if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT64)) {
        return QVariant::fromValue<ulong>(g_variant_get_uint64(variant));
    }

    return QVariant();
}

GVariant *PortalPrivate::buttonsToVariant(const NotificationButtons &buttons)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("aa{sv}"));

    for (const NotificationButton &button : buttons) {
        if (!button.isValid()) {
            continue;
        }

        GVariantBuilder buttonBuilder;
        g_variant_builder_init(&buttonBuilder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&buttonBuilder, "{sv}", "label", g_variant_new_string(button.label().toUtf8().constData()));
        g_variant_builder_add(&buttonBuilder, "{sv}", "action", g_variant_new_string(button.action().toUtf8().constData()));

        if (!button.target().isNull()) {
            g_variant_builder_add(&buttonBuilder, "{sv}", "target", convertVariant(button.target()));
        }

        g_variant_builder_add(&builder, "@a{sv}", g_variant_builder_end(&buttonBuilder));
    }

    return g_variant_builder_end(&builder);
}

GVariant *PortalPrivate::notificationToVariant(const Notification &notification)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    if (!notification.title().isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "title", g_variant_new_string(notification.title().toUtf8().constData()));
    }

    if (!notification.body().isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "body", g_variant_new_string(notification.body().toUtf8().constData()));
    }

    if (!notification.icon().isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "icon", g_icon_serialize(g_themed_icon_new(notification.icon().toUtf8().constData())));
    } else if (!notification.pixmap().isNull()) {
        g_autoptr(GBytes) bytes = nullptr;
        QByteArray array;
        QBuffer buffer(&array);
        buffer.open(QIODevice::WriteOnly);
        notification.pixmap().save(&buffer, "PNG");
        bytes = g_bytes_new(array.data(), array.size());
        g_variant_builder_add(&builder, "{sv}", "icon", g_icon_serialize(g_bytes_icon_new(bytes)));
    }

    if (!notification.defaultAction().isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "default-action", g_variant_new_string(notification.defaultAction().toUtf8().constData()));
    }

    if (!notification.defaultTarget().isNull()) {
        g_variant_builder_add(&builder, "{sv}", "default-action-target", convertVariant(notification.defaultTarget()));
    }

    if (!notification.buttons().isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "buttons", buttonsToVariant(notification.buttons()));
    }

    return g_variant_builder_end(&builder);
}

void PortalPrivate::addNotification(const QString &id, const Notification &notification, NotificationFlags flags)
{
    xdp_portal_add_notification(m_xdpPortal, id.toUtf8().constData(), notificationToVariant(notification),
                                static_cast<XdpNotificationFlags>((int)flags), nullptr /*cancellable*/, notificationAdded, this);
}

void PortalPrivate::notificationAdded(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);
    g_autoptr(GError) error = nullptr;

    bool ret = xdp_portal_add_notification_finish(xdpPortal, result, &error);

    Response response(ret, error);

    portalPrivate->addNotificationResponse(response);
}

void PortalPrivate::removeNotification(const QString &id)
{
    xdp_portal_remove_notification(m_xdpPortal, id.toUtf8().constData());
}

void PortalPrivate::onNotificationActionInvoked(XdpPortal *portal, const char *id, const char *action, GVariant *parameter,  gpointer data)
{
    Q_UNUSED(portal)

    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);

    portalPrivate->notificationActionInvoked(QString(id), QString(action), convertVariant(parameter));
}

}
