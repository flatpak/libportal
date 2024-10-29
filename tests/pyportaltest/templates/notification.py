# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from pyportaltest.templates import Request, Response, ASVType, MockParams
from typing import Dict, List, Tuple, Iterator

import dbus
import dbus.service
import logging

logger = logging.getLogger(f"templates.{__name__}")

BUS_NAME = "org.freedesktop.portal.Desktop"
MAIN_OBJ = "/org/freedesktop/portal/desktop"
SYSTEM_BUS = False
MAIN_IFACE = "org.freedesktop.portal.Notification"


def load(mock, parameters):
    logger.debug(f"loading {MAIN_IFACE} template")

    params = MockParams.get(mock, MAIN_IFACE)
    params.delay = 500
    params.version = parameters.get("version", 2)
    params.response = parameters.get("response", 0)

    mock.AddProperties(
        MAIN_IFACE,
        dbus.Dictionary({"version": dbus.UInt32(params.version)}),
    )


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="sa{sv}",
    out_signature="",
)
def AddNotification(self, id, notification, sender):
    try:
        logger.debug(f"AddNotification: {id}, {notification}")

    except Exception as e:
        logger.critical(e)
