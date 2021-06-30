/*
 * Copyright (C) 2021, Jan Grulich
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

#include "portal_p.h"
#include "portal.h"
#include "qttest.h"

#include <QSignalSpy>
#include <QTest>

void QtTest::testFileChooserPortal()
{
    Xdp::FileChooserFilterRule rule(Xdp::FileChooserFilterRule::Type::Mimetype, QStringLiteral("image/jpeg"));
    Xdp::FileChooserFilter filter(QStringLiteral("Images"), {rule});

    g_autoptr(GVariant) filterVar = Xdp::PortalPrivate::filterToVariant(filter);
    const QString expectedFilterVarStr = "('Images', [(1, 'image/jpeg')])";
    const QString filterVarStr = g_variant_print(filterVar, false);
    QCOMPARE(expectedFilterVarStr, filterVarStr);


    Xdp::FileChooserFilterRule rule2;
    rule2.setType(Xdp::FileChooserFilterRule::Type::Pattern);
    rule2.setRule(QStringLiteral("*.png"));
    filter.addRule(rule2);

    g_autoptr(GVariant) filterVar2 = Xdp::PortalPrivate::filterToVariant(filter);
    const QString expectedFilterVarStr2 = "('Images', [(1, 'image/jpeg'), (0, '*.png')])";
    const QString filterVarStr2 = g_variant_print(filterVar2, false);
    QCOMPARE(expectedFilterVarStr2, filterVarStr2);

    Xdp::FileChooserFilterList filterList;
    filterList << filter << filter;

    g_autoptr(GVariant) filterListVar = Xdp::PortalPrivate::filterListToVariant(filterList);
    const QString expectedFilterListVarStr = "[('Images', [(1, 'image/jpeg'), (0, '*.png')]), ('Images', [(1, 'image/jpeg'), (0, '*.png')])]";
    const QString filterListVarStr = g_variant_print(filterListVar, false);
    QCOMPARE(expectedFilterListVarStr, filterListVarStr);

    Xdp::FileChooserChoice choice(QStringLiteral("choice-id"), QStringLiteral("choice-label"),
                                  QMap<QString, QString>{{QStringLiteral("option1-id"), QStringLiteral("option1-value")}}, QStringLiteral("option1-id"));
    choice.addOption(QStringLiteral("option2-id"), QStringLiteral("option2-value"));

    g_autoptr(GVariant) choiceVar = Xdp::PortalPrivate::choiceToVariant(choice);
    const QString expectedChoiceVarStr = "('choice-id', 'choice-label', [('option1-id', 'option1-value'), ('option2-id', 'option2-value')], 'option1-id')";
    const QString choiceVarStr = g_variant_print(choiceVar, false);
    QCOMPARE(expectedChoiceVarStr, choiceVarStr);
}

void QtTest::testNotificationPortal()
{
    Xdp::NotificationButton button(QStringLiteral("Some label"), QStringLiteral("Some action"));

    g_autoptr(GVariant) buttonsVar = Xdp::PortalPrivate::buttonsToVariant({button});
    const QString expectedButtonsVarStr = "[{'label': <'Some label'>, 'action': <'Some action'>}]";
    const QString buttonsVarStr = g_variant_print(buttonsVar, false);
    QCOMPARE(expectedButtonsVarStr, buttonsVarStr);

    Xdp::Notification notification(QStringLiteral("Test notification"), QStringLiteral("Testing notification portal"));
    notification.setIcon(QStringLiteral("applications-development"));
    notification.addButton(button);

    g_autoptr(GVariant) notificationVar = Xdp::PortalPrivate::notificationToVariant(notification);
    const QString expectedNotificationVarStr = "{'title': <'Test notification'>, 'body': <'Testing notification portal'>, 'icon': <('themed', <['applications-development', 'applications-development-symbolic']>)>, 'buttons': <[{'label': <'Some label'>, 'action': <'Some action'>}]>}";
    const QString notificationStr = g_variant_print(notificationVar, false);
    QCOMPARE(expectedNotificationVarStr, notificationStr);
}


QTEST_MAIN(QtTest)
