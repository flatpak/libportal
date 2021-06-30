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

#ifndef LIBPORTALQT_RESPONSE_H
#define LIBPORTALQT_RESPONSE_H

#undef signals
#include <gio/gio.h>
#define signals Q_SIGNALS

#include <QVariantMap>

#include "libportalqt_export.h"

namespace Xdp
{

class ResponsePrivate;

class LIBPORTALQT_EXPORT Response : public QObject
{
Q_OBJECT
public:
    Q_PROPERTY(bool isError READ isError)
    Q_PROPERTY(bool isSuccess READ isSuccess)
    Q_PROPERTY(QString errorMessage READ errorMessage)
    Q_PROPERTY(QVariantMap result READ result)

    explicit Response(bool success, const GError *error, const QVariantMap &result = QVariantMap(), QObject *parent = nullptr);
    virtual ~Response();

    bool isError() const;

    bool isSuccess() const;

    QString errorMessage() const;

    QVariantMap result() const;

private:
    Q_DECLARE_PRIVATE(Response)

    const QScopedPointer<ResponsePrivate> d_ptr;
};
} // namespace Xdp

#endif // LIBPORTALQT_RESPONSE_H

