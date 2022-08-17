# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from dbusmock import DBusMockObject
from typing import Dict, Any, NamedTuple, Optional
from itertools import count
from gi.repository import GLib

import dbus
import logging


ASVType = Dict[str, Any]

logging.basicConfig(format="%(levelname).1s|%(name)s: %(message)s", level=logging.DEBUG)
logger = logging.getLogger("templates")


class MockParams:
    """
    Helper class for storing template parameters. The Mock object passed into
    ``load()`` is shared between all templates. This makes it easy to have
    per-template parameters by calling:

        >>> params = MockParams.get(mock, MAIN_IFACE)
        >>> params.version = 1

    and later, inside a DBus method:
        >>> params = MockParams.get(self, MAIN_IFACE)
        >>> return params.version
    """

    @classmethod
    def get(cls, mock, interface_name):
        params = getattr(mock, "params", {})
        try:
            return params[interface_name]
        except KeyError:
            c = cls()
            params[interface_name] = c
            mock.params = params
            return c


class Response(NamedTuple):
    response: int
    results: ASVType


class Request:
    _token_counter = count()

    def __init__(
        self, bus_name: dbus.service.BusName, sender: str, options: Optional[ASVType]
    ):
        options = options or {}
        sender_token = sender.removeprefix(":").replace(".", "_")
        handle_token = options.get("handle_token", next(self._token_counter))
        self.sender = sender
        self.handle = (
            f"/org/freedesktop/portal/desktop/request/{sender_token}/{handle_token}"
        )
        self.mock = DBusMockObject(
            bus_name=bus_name,
            path=self.handle,
            interface="org.freedesktop.portal.Request",
            props={},
        )
        self.mock.AddMethod("", "Close", "", "", "self.RemoveObject(self.path)")
        logger.debug(f"Request created at {self.handle}")

    def respond(self, response: Response, delay: int = 0):
        def respond():
            logger.debug(f"Request.Response on {self.handle}: {response}")
            self.mock.EmitSignalDetailed(
                "",
                "Response",
                "ua{sv}",
                [dbus.UInt32(response.response), response.results],
                details={"destination": self.sender},
            )

        if delay > 0:
            GLib.timeout_add(delay, respond)
        else:
            respond()


class Session:
    _token_counter = count()

    def __init__(
        self, bus_name: dbus.service.BusName, sender: str, options: Optional[ASVType]
    ):
        options = options or {}
        sender_token = sender.removeprefix(":").replace(".", "_")
        handle_token = options.get("session_handle_token", next(self._token_counter))
        self.sender = sender
        self.handle = (
            f"/org/freedesktop/portal/desktop/session/{sender_token}/{handle_token}"
        )
        self.mock = DBusMockObject(
            bus_name=bus_name,
            path=self.handle,
            interface="org.freedesktop.portal.Session",
            props={},
        )
        self.mock.AddMethod("", "Close", "", "", "self.RemoveObject(self.path)")
        logger.debug(f"Session created at {self.handle}")

    def close(self, details: ASVType, delay: int = 0):
        def respond():
            logger.debug(f"Session.Closed on {self.handle}: {details}")
            self.mock.EmitSignalDetailed(
                "", "Closed", "a{sv}", [details], destination=self.sender
            )

        if delay > 0:
            GLib.timeout_add(delay, respond)
        else:
            respond()
