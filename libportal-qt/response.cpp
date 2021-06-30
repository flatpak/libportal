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

#include "response.h"

namespace Xdp
{

class ResponsePrivate {
public:
    ResponsePrivate(bool success, const QString &errorMessage, const QVariantMap &result = QVariantMap());
    ~ResponsePrivate();

    bool m_success = false;
    QString m_errorMessage;
    QVariantMap m_result;
};

ResponsePrivate::ResponsePrivate(bool success, const QString &errorMessage, const QVariantMap &result)
    : m_success(success)
    , m_errorMessage(errorMessage)
    , m_result(result)
{
}

ResponsePrivate::~ResponsePrivate()
{
}

Response::Response(bool success, const QString &errorMessage, const QVariantMap &result)
    : d_ptr(new ResponsePrivate(success, errorMessage, result))
{
}

Response::~Response()
{
}

bool Response::isError() const
{
    Q_D(const Response);
    return !d->m_success;
}

bool Response::isSuccess() const
{
    Q_D(const Response);
    return d->m_success;
}

QString Response::errorMessage() const
{
    Q_D(const Response);
    return d->m_errorMessage;
}

QVariantMap Response::result() const
{
    Q_D(const Response);
    return d->m_result;
}

}
