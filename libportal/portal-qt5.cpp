/*
 * Copyright (C) 2021, Georges Basile Stavracas Neto
                 2020-2022, Jan Grulich
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

#include "config.h"
#include "portal-qt5.h"

#include "parent-private.h"

#include <QBuffer>
#include <QX11Info>

static gboolean
_xdp_parent_export_qt (XdpParent *parent,
                       XdpParentExported callback,
                       gpointer data)
{
  if (QX11Info::isPlatformX11 ())
    {
      QWindow *w = (QWindow *) parent->data;
      if (w) {
        guint32 xid = (guint32) w->winId ();
        g_autofree char *handle = g_strdup_printf ("x11:%x", xid);
        callback (parent, handle, data);
        return TRUE;
      }
    }
  else
    {
      /* TODO: QtWayland doesn't support xdg-foreign protocol yet
       * Upstream bugs: https://bugreports.qt.io/browse/QTBUG-73801
       *                https://bugreports.qt.io/browse/QTBUG-76983
       */
      g_warning ("QtWayland doesn't support xdg-foreign protocol yet");
      g_autofree char *handle = g_strdup ("");
      callback (parent, handle, data);
      return TRUE;
    }

  g_warning ("Couldn't export handle, unsupported windowing system");
  return FALSE;
}

static inline void _xdp_parent_unexport_qt (XdpParent *parent)
{
}

XdpParent *
xdp_parent_new_qt (QWindow *window)
{
  XdpParent *parent = g_new0 (XdpParent, 1);
  parent->parent_export = _xdp_parent_export_qt;
  parent->parent_unexport = _xdp_parent_unexport_qt;
  parent->data = (gpointer) window;
  return parent;
}

namespace XdpQt {

class LibPortalQt5 {
public:
    LibPortalQt5() : m_xdpPortal(xdp_portal_new()) { }
    ~LibPortalQt5() { if (m_xdpPortal) { g_object_unref(m_xdpPortal); } }
    XdpPortal *portalObject() const { return m_xdpPortal; }
private:
    XdpPortal *m_xdpPortal = nullptr;
};

Q_GLOBAL_STATIC(LibPortalQt5, globalLibPortalQt5)

XdpPortal*
globalPortalObject()
{
    return globalLibPortalQt5->portalObject();
}

GetUserInformationResult
getUserInformationResultFromGVariant(GVariant *variant)
{
    GetUserInformationResult result;

    g_autofree gchar *id = nullptr;
    g_autofree gchar *name = nullptr;
    g_autofree gchar *image = nullptr;

    if (variant) {
        if (g_variant_lookup(variant, "id", "s", &id)) {
            result.id = id;
        }

        if (g_variant_lookup(variant, "name", "s", &name)) {
            result.name = name;
        }

        if (g_variant_lookup(variant, "image", "s", &image)) {
            result.image = image;
        }
    }

    return result;
}

GVariant *
filechooserFilterToGVariant(const FileChooserFilter &filter)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(us)"));

    for (const FileChooserFilterRule &rule : filter.rules) {
        g_variant_builder_add(&builder, "(us)", static_cast<uint>(rule.type), rule.rule.toUtf8().constData());
    }

    return g_variant_new("(s@a(us))", filter.label.toUtf8().constData(), g_variant_builder_end(&builder));
}

GVariant *
filechooserFiltersToGVariant(const QList<FileChooserFilter> &filters)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(sa(us))"));

    for (const FileChooserFilter &filter : filters) {
        g_variant_builder_add(&builder, "@(sa(us))", filechooserFilterToGVariant(filter));
    }

    return g_variant_builder_end(&builder);
}

static GVariant *
filechooserChoiceToGVariant(const FileChooserChoice &choice)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));

    if (choice.options.count()) {
        for (auto it = choice.options.constBegin(); it != choice.options.constEnd(); ++it) {
            g_variant_builder_add(&builder, "(&s&s)", it.key().toUtf8().constData(), it.value().toUtf8().constData());
        }
    }

    return g_variant_new("(&s&s@a(ss)&s)", choice.id.toUtf8().constData(), choice.label.toUtf8().constData(),
                         g_variant_builder_end(&builder), choice.selected.toUtf8().constData());
}

GVariant *
filechooserChoicesToGVariant(const QList<FileChooserChoice> &choices)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ssa(ss)s)"));

    for (const FileChooserChoice &choice : choices) {
        g_variant_builder_add(&builder, "@(ssa(ss)s)", filechooserChoiceToGVariant(choice));
    }

    return g_variant_builder_end(&builder);
}


GVariant *
filechooserFilesToGVariant(const QStringList &files)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE_BYTESTRING_ARRAY);

    for (const QString &file : files) {
        g_variant_builder_add(&builder, "@ay", g_variant_new_bytestring(file.toUtf8().constData()));
    }

    return g_variant_builder_end(&builder);
}

FileChooserResult
filechooserResultFromGVariant(GVariant *variant)
{
    FileChooserResult result;

    g_autofree const char **uris = nullptr;
    g_autoptr(GVariant) choices = nullptr;

    if (variant) {
        g_variant_lookup(variant, "uris", "^a&s", &uris);

        choices = g_variant_lookup_value(variant, "choices", G_VARIANT_TYPE("a(ss)"));
        if (choices) {
            QMap<QString, QString> choicesMap;
            for (uint i = 0; i < g_variant_n_children(choices); i++) {
                const char *id;
                const char *selected;
                g_variant_get_child(choices, i, "(&s&s)", &id, &selected);
                result.choices.insert(QString(id), QString(selected));
            }
            g_variant_unref (choices);
        }

        for (int i = 0; uris[i]; i++) {
            result.uris << QString(uris[i]);
        }
    }

    return result;
}

static GVariant*
QVariantToGVariant(const QVariant &variant)
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

QVariant
GVariantToQVariant(GVariant *variant)
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

static GVariant *
notificationButtonsToGVariant(const QList<NotificationButton> &buttons)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("aa{sv}"));

    for (const NotificationButton &button : buttons) {
        GVariantBuilder buttonBuilder;
        g_variant_builder_init(&buttonBuilder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&buttonBuilder, "{sv}", "label", g_variant_new_string(button.label.toUtf8().constData()));
        g_variant_builder_add(&buttonBuilder, "{sv}", "action", g_variant_new_string(button.action.toUtf8().constData()));

        if (!button.target.isNull()) {
            g_variant_builder_add(&buttonBuilder, "{sv}", "target", QVariantToGVariant(button.target));
        }

        g_variant_builder_add(&builder, "@a{sv}", g_variant_builder_end(&buttonBuilder));
    }

    return g_variant_builder_end(&builder);
}

GVariant *
notificationToGVariant(const Notification &notification) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

    if (!notification.title.isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "title", g_variant_new_string(notification.title.toUtf8().constData()));
    }

    if (!notification.body.isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "body", g_variant_new_string(notification.body.toUtf8().constData()));
    }

    if (!notification.icon.isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "icon", g_icon_serialize(g_themed_icon_new(notification.icon.toUtf8().constData())));
    } else if (!notification.pixmap.isNull()) {
        g_autoptr(GBytes) bytes = nullptr;
        QByteArray array;
        QBuffer buffer(&array);
        buffer.open(QIODevice::WriteOnly);
        notification.pixmap.save(&buffer, "PNG");
        bytes = g_bytes_new(array.data(), array.size());
        g_variant_builder_add(&builder, "{sv}", "icon", g_icon_serialize(g_bytes_icon_new(bytes)));
    }

    if (!notification.defaultAction.isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "default-action", g_variant_new_string(notification.defaultAction.toUtf8().constData()));
    }

    if (!notification.defaultTarget.isNull()) {
        g_variant_builder_add(&builder, "{sv}", "default-action-target", QVariantToGVariant(notification.defaultTarget));
    }

    if (!notification.buttons.isEmpty()) {
        g_variant_builder_add(&builder, "{sv}", "buttons", notificationButtonsToGVariant(notification.buttons));
    }

    return g_variant_builder_end(&builder);
}

}
