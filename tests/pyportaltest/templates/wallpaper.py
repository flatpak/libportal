# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from pyportaltest.templates import Request, Response, ASVType
from typing import Dict, List, Tuple, Iterator

import dbus
import dbus.service
import logging

logger = logging.getLogger(f"templates.{__name__}")

BUS_NAME = "org.freedesktop.portal.Desktop"
MAIN_OBJ = "/org/freedesktop/portal/desktop"
SYSTEM_BUS = False
MAIN_IFACE = "org.freedesktop.portal.Wallpaper"


def load(mock, parameters):
    logger.debug(f"loading {MAIN_IFACE} template")
    mock.delay = 500

    mock.response = parameters.get("response", 0)

    mock.AddProperties(
        MAIN_IFACE,
        dbus.Dictionary({"version": dbus.UInt32(parameters.get("version", 1))}),
    )


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="ssa{sv}",
    out_signature="o",
)
def SetWallpaperURI(self, parent_window, uri, options, sender):
    try:
        logger.debug(f"SetWallpaperURI: {parent_window}, {uri}, {options}")
        request = Request(bus_name=self.bus_name, sender=sender, options=options)

        response = Response(self.response, {})

        request.respond(response, delay=self.delay)

        return request.handle
    except Exception as e:
        logger.critical(e)
