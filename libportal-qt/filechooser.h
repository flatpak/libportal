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

#ifndef LIBPORTALQT_FILECHOOSER_H
#define LIBPORTALQT_FILECHOOSER_H

#include "libportalqt_export.h"

#include <QFlags>
#include <QMap>
#include <QStringList>
#include <QSharedPointer>

namespace Xdp
{

enum class OpenFileFlag
{
    None = 0x0,
    Multiple = 0x1
};
Q_DECLARE_FLAGS(OpenFileFlags, OpenFileFlag)

enum class SaveFileFlag
{
    None = 0x0
};
Q_DECLARE_FLAGS(SaveFileFlags, SaveFileFlag)

class FileChooserFilterRulePrivate;

class LIBPORTALQT_EXPORT FileChooserFilterRule
{
public:
    enum class Type {
        Pattern = 0,
        Mimetype = 1
    };

    explicit FileChooserFilterRule();
    FileChooserFilterRule(Type type, const QString &rule);
    FileChooserFilterRule(const FileChooserFilterRule &other);
    ~FileChooserFilterRule();

    void operator=(const FileChooserFilterRule &other);

    bool isValid() const;

    Type type() const;
    void setType(Type type);

    QString rule() const;
    void setRule(const QString &rule);
private:
    Q_DECLARE_PRIVATE(FileChooserFilterRule)

    const QScopedPointer<FileChooserFilterRulePrivate> d_ptr;
};
typedef QList<FileChooserFilterRule> FileChooserFilterRules;

class FileChooserFilterPrivate;

class LIBPORTALQT_EXPORT FileChooserFilter
{
public:
    explicit FileChooserFilter();
    FileChooserFilter(const QString &label, const FileChooserFilterRules &rules);
    FileChooserFilter(const FileChooserFilter &other);
    ~FileChooserFilter();

    void operator=(const FileChooserFilter &other);

    bool isValid() const;

    QString label() const;
    void setLabel(const QString &label);

    FileChooserFilterRules rules() const;
    void addRule(const FileChooserFilterRule &rule);
private:
    Q_DECLARE_PRIVATE(FileChooserFilter)

    const QScopedPointer<FileChooserFilterPrivate> d_ptr;
};
typedef QList<FileChooserFilter> FileChooserFilterList;

class FileChooserChoicePrivate;

class LIBPORTALQT_EXPORT FileChooserChoice
{
public:
    explicit FileChooserChoice();
    FileChooserChoice(const QString &id, const QString &label, const QMap<QString, QString> &options, const QString &selected = QString());
    FileChooserChoice(const FileChooserChoice &other);
    ~FileChooserChoice();

    void operator=(const FileChooserChoice &other);

    bool isValid() const;

    QString id() const;
    void setId(const QString &id);

    QString label() const;
    void setLabel(const QString &label);

    QMap<QString, QString> options() const;
    void addOption(const QString &id, const QString &label);

    QString selected() const;
    void setSelected(const QString &selected);

private:
    Q_DECLARE_PRIVATE(FileChooserChoice)

    const QScopedPointer<FileChooserChoicePrivate> d_ptr;
};
typedef QList<FileChooserChoice> FileChooserChoices;

}
Q_DECLARE_OPERATORS_FOR_FLAGS(Xdp::OpenFileFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Xdp::SaveFileFlags)

#endif // LIBPORTALQT_FILECHOOSER_H
