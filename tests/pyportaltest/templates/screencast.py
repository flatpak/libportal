# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from pyportaltest.templates import Request, Response, Session, ASVType
from typing import Dict, List, Tuple, Iterator
from itertools import count

import dbus.service
import logging
import socket

logger = logging.getLogger(f"templates.{__name__}")

BUS_NAME = "org.freedesktop.portal.Desktop"
MAIN_OBJ = "/org/freedesktop/portal/desktop"
SYSTEM_BUS = False
MAIN_IFACE = "org.freedesktop.portal.ScreenCast"

_restore_tokens = count()


def load(mock, parameters=None):
    logger.debug(f"loading {MAIN_IFACE} template")

    mock.delay = 500
    mock.version = parameters.get("version", 4)

    mock.response = parameters.get("response", 0)

    # streams returned in Start
    mock.streams = parameters.get("streams", [])

    # persist_mode returned in Start
    mock.persist_mode = parameters.get("persist-mode", 0)

    mock.AddProperties(
        MAIN_IFACE,
        dbus.Dictionary(
            {
                "version": dbus.UInt32(mock.version),
                "AvailableSourceTypes": dbus.UInt32(
                    parameters.get("source-types", 0b111)
                ),
                "AvailableCursorModes": dbus.UInt32(
                    parameters.get("cursor-modes", 0b111)
                ),
            }
        ),
    )

    mock.sessions: Dict[str, Session] = {}


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="a{sv}",
    out_signature="o",
)
def CreateSession(self, options, sender):
    try:
        logger.debug(f"CreateSession: {options}")
        request = Request(bus_name=self.bus_name, sender=sender, options=options)

        session = Session(bus_name=self.bus_name, sender=sender, options=options)
        self.sessions[session.handle] = session

        response = Response(self.response, {})

        request.respond(response, delay=self.delay)

        return request.handle
    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="oa{sv}",
    out_signature="o",
)
def SelectSources(self, session_handle, options, sender):
    try:
        logger.debug(f"SelectSources: {session_handle} {options}")
        request = Request(bus_name=self.bus_name, sender=sender, options=options)

        response = Response(self.response, {})

        request.respond(response, delay=self.delay)

        return request.handle
    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="osa{sv}",
    out_signature="o",
)
def Start(self, session_handle, parent_window, options, sender):
    try:
        logger.debug(f"Start: {session_handle} {options}")
        request = Request(bus_name=self.bus_name, sender=sender, options=options)

        results = {
            "streams": self.streams,
        }

        if self.version >= 4:
            results["persist_mode"] = dbus.UInt32(self.persist_mode)
            if self.persist_mode != 0:
                results["restore_token"] = f"restore_token{next(_restore_tokens)}"

        response = Response(self.response, results)

        request.respond(response, delay=self.delay)

        return request.handle
    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="oa{sv}",
    out_signature="h",
)
def OpenPipeWireRemote(self, session_handle, options, sender):
    try:
        logger.debug(f"OpenPipeWireRemote: {session_handle} {options}")

        # libportal doesn't care about the socket, so let's use something we
        # can easily check
        sockets = socket.socketpair()

        pw_socket = sockets[0]
        pw_socket.send(b"I AM GROO^WPIPEWIRE")

        fd = sockets[1]

        logger.debug(f"OpenPipeWireRemote with fd {fd.fileno()}")

        return dbus.types.UnixFd(fd)
    except Exception as e:
        logger.critical(e)
