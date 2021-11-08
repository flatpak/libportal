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

#ifndef LIBPORTALQT_EMAIL_H
#define LIBPORTALQT_EMAIL_H

#include <QFlags>

namespace Xdp
{

enum class EmailFlag
{
    None = 0x0
};
Q_DECLARE_FLAGS(EmailFlags, EmailFlag)
}
Q_DECLARE_OPERATORS_FOR_FLAGS(Xdp::EmailFlags)

#endif // LIBPORTALQT_EMAIL_H



