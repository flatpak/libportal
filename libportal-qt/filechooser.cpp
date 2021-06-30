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

#include "filechooser.h"
#include "portal.h"
#include "parent.h"
#include "parent_p.h"
#include "portal_p.h"

#include <QFile>

namespace Xdp
{

class FileChooserFilterRulePrivate
{
public:
    FileChooserFilterRulePrivate();
    FileChooserFilterRulePrivate(FileChooserFilterRule::Type type, const QString &rule);
    FileChooserFilterRulePrivate(const FileChooserFilterRulePrivate &other);
    ~FileChooserFilterRulePrivate() { };

    FileChooserFilterRule::Type m_type = FileChooserFilterRule::Type::Pattern;
    QString m_rule;
};

FileChooserFilterRulePrivate::FileChooserFilterRulePrivate()
{
}

FileChooserFilterRulePrivate::FileChooserFilterRulePrivate(FileChooserFilterRule::Type type, const QString &rule)
    : m_type(type)
    , m_rule(rule)
{
}

FileChooserFilterRulePrivate::FileChooserFilterRulePrivate(const FileChooserFilterRulePrivate &other)
    : FileChooserFilterRulePrivate(other.m_type, other.m_rule)
{
}

FileChooserFilterRule::FileChooserFilterRule()
    : d_ptr(new FileChooserFilterRulePrivate())
{
}

FileChooserFilterRule::FileChooserFilterRule(FileChooserFilterRule::Type type, const QString &rule)
    : d_ptr(new FileChooserFilterRulePrivate(type, rule))
{
}


FileChooserFilterRule::FileChooserFilterRule(const FileChooserFilterRule &other)
    : FileChooserFilterRule(other.type(), other.rule())
{
}

FileChooserFilterRule::~FileChooserFilterRule()
{
}

void FileChooserFilterRule::operator=(const FileChooserFilterRule &other)
{
    setRule(other.rule());
    setType(other.type());
}

bool FileChooserFilterRule::isValid() const
{
    Q_D(const FileChooserFilterRule);
    return !d->m_rule.isEmpty();
}

FileChooserFilterRule::Type FileChooserFilterRule::type() const
{
    Q_D(const FileChooserFilterRule);
    return d->m_type;
}

void FileChooserFilterRule::setType(FileChooserFilterRule::Type type)
{
    Q_D(FileChooserFilterRule);
    d->m_type = type;
}

QString FileChooserFilterRule::rule() const
{
    Q_D(const FileChooserFilterRule);
    return d->m_rule;
}

void FileChooserFilterRule::setRule(const QString &rule)
{
    Q_D(FileChooserFilterRule);
    d->m_rule = rule;
}

class FileChooserFilterPrivate
{
public:
    FileChooserFilterPrivate() { };
    FileChooserFilterPrivate(const QString &label, const FileChooserFilterRules &rules);
    FileChooserFilterPrivate(const FileChooserFilterPrivate &other);
    ~FileChooserFilterPrivate() { };

    QString m_label;
    FileChooserFilterRules m_rules;
};

FileChooserFilterPrivate::FileChooserFilterPrivate(const QString &label, const FileChooserFilterRules &rules)
    : m_label(label)
    , m_rules(rules)
{
}

FileChooserFilterPrivate::FileChooserFilterPrivate(const FileChooserFilterPrivate &other)
    : FileChooserFilterPrivate(other.m_label, other.m_rules)
{
}

FileChooserFilter::FileChooserFilter()
    : d_ptr(new FileChooserFilterPrivate())
{
}

FileChooserFilter::FileChooserFilter(const QString &label, const FileChooserFilterRules &rules)
    : d_ptr(new FileChooserFilterPrivate(label, rules))
{
}

FileChooserFilter::FileChooserFilter(const FileChooserFilter &other)
    : FileChooserFilter(other.label(), other.rules())
{
}

FileChooserFilter::~FileChooserFilter()
{
}

void FileChooserFilter::operator=(const FileChooserFilter &other)
{
    setLabel(other.label());
    for (const FileChooserFilterRule &rule : other.rules()) {
        addRule(rule);
    }
}

bool FileChooserFilter::isValid() const
{
    Q_D(const FileChooserFilter);
    if (d->m_label.isEmpty() || d->m_rules.isEmpty()) {
        return false;
    }

    for (const FileChooserFilterRule &rule : d->m_rules) {
        if (!rule.isValid()) {
            return false;
        }
    }

    return true;
}

QString FileChooserFilter::label() const
{
    Q_D(const FileChooserFilter);
    return d->m_label;
}

void FileChooserFilter::setLabel(const QString &label)
{
    Q_D(FileChooserFilter);
    d->m_label = label;
}

FileChooserFilterRules FileChooserFilter::rules() const
{
    Q_D(const FileChooserFilter);
    return d->m_rules;
}

void FileChooserFilter::addRule(const FileChooserFilterRule &rule)
{
    Q_D(FileChooserFilter);
    d->m_rules << rule;
}

class FileChooserChoicePrivate
{
public:
    FileChooserChoicePrivate();
    FileChooserChoicePrivate(const QString &id, const QString &label, const QMap<QString, QString> &options, const QString &selected);
    FileChooserChoicePrivate(const FileChooserChoicePrivate &other);
    ~FileChooserChoicePrivate() { };

    QString m_id;
    QString m_label;
    QMap<QString, QString> m_options;
    QString m_selected;
};

FileChooserChoicePrivate::FileChooserChoicePrivate()
{
}

FileChooserChoicePrivate::FileChooserChoicePrivate(const QString &id, const QString &label, const QMap<QString, QString> &options, const QString &selected)
    : m_id(id)
    , m_label(label)
    , m_options(options)
    , m_selected(selected)
{
}

FileChooserChoicePrivate::FileChooserChoicePrivate(const FileChooserChoicePrivate &other)
    : FileChooserChoicePrivate(other.m_id, other.m_label, other.m_options, other.m_selected)
{
}

FileChooserChoice::FileChooserChoice()
    : d_ptr(new FileChooserChoicePrivate())
{
}

FileChooserChoice::FileChooserChoice(const QString &id, const QString &label, const QMap<QString, QString> &choices, const QString &selected)
    : d_ptr(new FileChooserChoicePrivate(id, label, choices, selected))
{
}

FileChooserChoice::FileChooserChoice(const FileChooserChoice &other)
    : FileChooserChoice(other.id(), other.label(), other.options(), other.selected())
{
}

FileChooserChoice::~FileChooserChoice()
{
}

void FileChooserChoice::operator=(const FileChooserChoice &other)
{
    setId(other.id());
    setLabel(other.label());
    setSelected(other.selected());

    QMap<QString, QString>::iterator it = other.options().begin();
    while (it != other.options().end()) {
        addOption(it.value(), it.key());
    }
}

bool FileChooserChoice::isValid() const
{
    Q_D(const FileChooserChoice);
    return !d->m_id.isEmpty() && !d->m_label.isEmpty() && !d->m_options.isEmpty();
}

QString FileChooserChoice::id() const
{
    Q_D(const FileChooserChoice);
    return d->m_id;
}

void FileChooserChoice::setId(const QString &id)
{
    Q_D(FileChooserChoice);
    d->m_id = id;
}

QString FileChooserChoice::label() const
{
    Q_D(const FileChooserChoice);
    return d->m_label;
}

void FileChooserChoice::setLabel(const QString &label)
{
    Q_D(FileChooserChoice);
    d->m_label = label;
}

QMap<QString, QString> FileChooserChoice::options() const
{
    Q_D(const FileChooserChoice);
    return d->m_options;
}

void FileChooserChoice::addOption(const QString &id, const QString &label)
{
    Q_D(FileChooserChoice);
    d->m_options.insert(id, label);
}

QString FileChooserChoice::selected() const
{
    Q_D(const FileChooserChoice);
    return d->m_selected;
}

void FileChooserChoice::setSelected(const QString& selected)
{
    Q_D(FileChooserChoice);
    d->m_selected = selected;
}

GVariant *PortalPrivate::filterToVariant(const FileChooserFilter &filter)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(us)"));

    for (const FileChooserFilterRule &rule : filter.rules()) {
        g_variant_builder_add(&builder, "(us)", static_cast<uint>(rule.type()), rule.rule().toUtf8().constData());
    }

    return g_variant_new("(s@a(us))", filter.label().toUtf8().constData(), g_variant_builder_end(&builder));
}

GVariant *PortalPrivate::filterListToVariant(const FileChooserFilterList &filters)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(sa(us))"));

    for (const FileChooserFilter &filter : filters) {
        g_variant_builder_add(&builder, "@(sa(us))", filterToVariant(filter));
    }

    return g_variant_builder_end(&builder);
}

GVariant *PortalPrivate::choiceToVariant(const FileChooserChoice &choice)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));

    if (choice.options().count()) {
        for (auto it = choice.options().constBegin(); it != choice.options().constEnd(); ++it) {
            g_variant_builder_add(&builder, "(&s&s)", it.key().toUtf8().constData(), it.value().toUtf8().constData());
        }
    }

    return g_variant_new("(&s&s@a(ss)&s)", choice.id().toUtf8().constData(), choice.label().toUtf8().constData(),
                         g_variant_builder_end(&builder), choice.selected().toUtf8().constData());
}

GVariant *PortalPrivate::choicesToVariant(const FileChooserChoices &choices)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ssa(ss)s)"));

    for (const FileChooserChoice &choice : choices) {
        g_variant_builder_add(&builder, "@(ssa(ss)s)",choiceToVariant(choice));
    }

    return g_variant_builder_end(&builder);
}

GVariant *PortalPrivate::filesToVariant(const QStringList &files)
{
    GVariantBuilder builder;

    g_variant_builder_init(&builder, G_VARIANT_TYPE_BYTESTRING_ARRAY);

    for (const QString &file : files) {
        g_variant_builder_add(&builder, "@ay", g_variant_new_bytestring(file.toUtf8().constData()));
    }

    return g_variant_builder_end(&builder);
}

void PortalPrivate::openFile(const Parent &parent, const QString &title, const FileChooserFilterList &filters, const FileChooserFilter &currentFilter,
                             const FileChooserChoices &choices, OpenFileFlags flags)
{
    xdp_portal_open_file(m_xdpPortal, parent.d_ptr->m_xdpParent, title.toUtf8().constData(), filterListToVariant(filters), currentFilter.isValid() ? filterToVariant(currentFilter) : nullptr,
                         choicesToVariant(choices), static_cast<XdpOpenFileFlags>((int)flags), nullptr /*cancellable*/, openedFile, this);
}

void PortalPrivate::saveFile(const Parent &parent, const QString &title, const QString &currentName, const QString &currentFolder, const QString &currentFile,
                             const FileChooserFilterList &filters, const FileChooserFilter &currentFilter, const FileChooserChoices &choices, SaveFileFlags flags)
{
    xdp_portal_save_file(m_xdpPortal, parent.d_ptr->m_xdpParent, title.toUtf8().constData(),  currentName.toUtf8().constData(),
                         QFile::encodeName(currentFolder).append('\0'), QFile::encodeName(currentFile).append('\0'),
                         filterListToVariant(filters), currentFilter.isValid() ? filterToVariant(currentFilter) : nullptr,
                         choicesToVariant(choices), static_cast<XdpSaveFileFlags>((int)flags), nullptr /*cancellable*/, savedFile, this);
}

void PortalPrivate::saveFiles(const Parent &parent, const QString &title, const QString &currentFolder, const QStringList &files,
                              const FileChooserChoices &choices, SaveFileFlags flags)
{
    xdp_portal_save_files(m_xdpPortal, parent.d_ptr->m_xdpParent, title.toUtf8().constData(), nullptr /*current_name*/,
                          QFile::encodeName(currentFolder).append('\0'), filesToVariant(files),
                          choicesToVariant(choices), static_cast<XdpSaveFileFlags>((int)flags), nullptr /*cancellable*/, savedFiles, this);
}

void PortalPrivate::openedFile(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);

    g_autofree const char **uris = nullptr;
    g_autoptr(GError) error = nullptr;
    GVariant *choices = nullptr;

    g_autoptr(GVariant) ret = xdp_portal_open_file_finish(xdpPortal, result, &error);

    if (ret) {
        QVariantMap responseData;
        g_variant_lookup(ret, "uris", "^a&s", &uris);

        choices = g_variant_lookup_value(ret, "choices", G_VARIANT_TYPE("a(ss)"));
        if (choices) {
            QMap<QString, QString> choicesMap;
            for (uint i = 0; i < g_variant_n_children(choices); i++) {
                const char *id;
                const char *selected;
                g_variant_get_child(choices, i, "(&s&s)", &id, &selected);
                choicesMap.insert(QString(id), QString(selected));
            }
            responseData.insert(QStringLiteral("choices"), QVariant::fromValue<QMap<QString, QString> >(choicesMap));
            g_variant_unref (choices);
        }

        QStringList uriList;
        for (int i = 0; uris[i]; i++) {
            uriList << QString(uris[i]);
        }
        responseData.insert(QStringLiteral("uris"), uriList);

        Response response(true, error, responseData);
        portalPrivate->openFileResponse(response);
    } else {
        Response response(false, error);
        portalPrivate->openFileResponse(response);
    }
}

void PortalPrivate::savedFile(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);

    g_autofree const char **uris = nullptr;
    g_autoptr(GError) error = nullptr;
    g_autoptr(GVariant) choices = nullptr;
    g_autoptr(GVariant) ret = xdp_portal_save_file_finish(xdpPortal, result, &error);

    if (ret) {
        QVariantMap responseData;
        g_variant_lookup(ret, "uris", "^a&s", &uris);

        choices = g_variant_lookup_value(ret, "choices", G_VARIANT_TYPE("a(ss)"));
        if (choices) {
            QMap<QString, QString> choicesMap;
            for (uint i = 0; i < g_variant_n_children(choices); i++) {
                const char *id;
                const char *selected;
                g_variant_get_child(choices, i, "(&s&s)", &id, &selected);
                choicesMap.insert(QString(id), QString(selected));
            }
            responseData.insert(QStringLiteral("choices"), QVariant::fromValue<QMap<QString, QString> >(choicesMap));
            g_variant_unref (choices);
        }

        QStringList uriList;
        for (int i = 0; uris[i]; i++) {
            uriList << QString(uris[i]);
        }
        responseData.insert(QStringLiteral("uris"), uriList);

        Response response(true, error, responseData);
        portalPrivate->saveFileResponse(response);
    } else {
        Response response(false, error);
        portalPrivate->saveFileResponse(response);
    }
}

void PortalPrivate::savedFiles(GObject *object, GAsyncResult *result, gpointer data)
{
    XdpPortal *xdpPortal = XDP_PORTAL(object);
    PortalPrivate *portalPrivate = static_cast<PortalPrivate*>(data);

    g_autofree const char **uris = nullptr;
    g_autoptr(GError) error = nullptr;
    g_autoptr(GVariant) choices = nullptr;
    g_autoptr(GVariant) ret = xdp_portal_save_files_finish(xdpPortal, result, &error);

    if (ret) {
        QVariantMap responseData;
        g_variant_lookup(ret, "uris", "^a&s", &uris);

        choices = g_variant_lookup_value(ret, "choices", G_VARIANT_TYPE("a(ss)"));
        if (choices) {
            QMap<QString, QString> choicesMap;
            for (uint i = 0; i < g_variant_n_children(choices); i++) {
                const char *id;
                const char *selected;
                g_variant_get_child(choices, i, "(&s&s)", &id, &selected);
                choicesMap.insert(QString(id), QString(selected));
            }
            responseData.insert(QStringLiteral("choices"), QVariant::fromValue<QMap<QString, QString> >(choicesMap));
            g_variant_unref (choices);
        }

        QStringList uriList;
        for (int i = 0; uris[i]; i++) {
            uriList << QString(uris[i]);
        }
        responseData.insert(QStringLiteral("uris"), uriList);

        Response response(true, error, responseData);
        portalPrivate->saveFilesResponse(response);
    } else {
        Response response(false, error);
        portalPrivate->saveFilesResponse(response);
    }
}

}
