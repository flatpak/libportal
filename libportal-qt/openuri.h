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

#ifndef LIBPORTALQT_OPENURI_H
#define LIBPORTALQT_OPENURI_H

#include <QFlags>

namespace Xdp
{

enum class OpenUriFlag
{
    None = 0x0,
    Ask = 0x1,
    Writable = 0x2
};
Q_DECLARE_FLAGS(OpenUriFlags, OpenUriFlag)
}
Q_DECLARE_OPERATORS_FOR_FLAGS(Xdp::OpenUriFlags)

#endif // LIBPORTALQT_OPENURI_H
