/*
 * Copyright (C) 2022, Jan Grulich
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

#include "test.h"

#undef signals
#include "portal-qt5.h"
#define signals Q_SIGNALS

#include <QSignalSpy>
#include <QTest>

void Test::testFileChooserPortal()
{
    XdpQt::FileChooserFilterRule rule;
    rule.type = XdpQt::FileChooserFilterRuleType::Mimetype;
    rule.rule = QStringLiteral("image/jpeg");

    XdpQt::FileChooserFilter filter;
    filter.label = QStringLiteral("Images");
    filter.rules << rule;

    g_autoptr(GVariant) filterVar = XdpQt::filechooserFiltersToGVariant({filter});
    const QString expectedFilterVarStr = QStringLiteral("[('Images', [(1, 'image/jpeg')])]");
    const QString filterVarStr = g_variant_print(filterVar, false);
    QCOMPARE(expectedFilterVarStr, filterVarStr);

    XdpQt::FileChooserFilterRule rule2;
    rule2.type = XdpQt::FileChooserFilterRuleType::Pattern;
    rule2.rule = QStringLiteral("*.png");
    filter.rules << rule2;

    g_autoptr(GVariant) filterVar2 = XdpQt::filechooserFiltersToGVariant({filter});
    const QString expectedFilterVarStr2 = "[('Images', [(1, 'image/jpeg'), (0, '*.png')])]";
    const QString filterVarStr2 = g_variant_print(filterVar2, false);
    QCOMPARE(expectedFilterVarStr2, filterVarStr2);

    XdpQt::FileChooserChoice choice;
    choice.id = QStringLiteral("choice-id");
    choice.label = QStringLiteral("choice-label");
    choice.options.insert(QStringLiteral("option1-id"), QStringLiteral("option1-value"));
    choice.options.insert(QStringLiteral("option2-id"), QStringLiteral("option2-value"));
    choice.selected = QStringLiteral("option1-id");

    g_autoptr(GVariant) choiceVar = XdpQt::filechooserChoicesToGVariant({choice});
    const QString expectedChoiceVarStr = "[('choice-id', 'choice-label', [('option1-id', 'option1-value'), ('option2-id', 'option2-value')], 'option1-id')]";
    const QString choiceVarStr = g_variant_print(choiceVar, false);
    QCOMPARE(expectedChoiceVarStr, choiceVarStr);
}

void Test::testNotificationPortal()
{
    XdpQt::NotificationButton button;
    button.label = QStringLiteral("Some label");
    button.action = QStringLiteral("Some action");

    XdpQt::Notification notification;
    notification.title = QStringLiteral("Test notification");
    notification.body = QStringLiteral("Testing notification portal");
    notification.icon = QStringLiteral("applications-development");
    notification.buttons << button;

    g_autoptr(GVariant) notificationVar = XdpQt::notificationToGVariant(notification);
    const QString expectedNotificationVarStr = "{'title': <'Test notification'>, 'body': <'Testing notification portal'>, 'icon': <('themed', <['applications-development', 'applications-development-symbolic']>)>, 'buttons': <[{'label': <'Some label'>, 'action': <'Some action'>}]>}";
    const QString notificationStr = g_variant_print(notificationVar, false);
    QCOMPARE(expectedNotificationVarStr, notificationStr);
}


QTEST_MAIN(Test)
