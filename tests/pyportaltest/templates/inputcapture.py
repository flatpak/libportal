# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

"""xdg desktop portals mock template"""

from pyportaltest.templates import Request, Response, ASVType, Session
from typing import Callable, Dict, List, Tuple, Iterator
from itertools import count

import dbus
import dbus.service
import logging
import sys

from gi.repository import GLib

BUS_NAME = "org.freedesktop.portal.Desktop"
MAIN_OBJ = "/org/freedesktop/portal/desktop"
SYSTEM_BUS = False
MAIN_IFACE = "org.freedesktop.portal.InputCapture"

logger = logging.getLogger(f"templates.{__name__}")
logger.setLevel(logging.DEBUG)

zone_set = None
eis_serial = None


def load(mock, parameters={}):
    logger.debug(f"Loading parameters: {parameters}")

    # Delay before Request.response, applies to all functions
    mock.delay: int = parameters.get("delay", 0)

    # EIS serial number, < 0 means "don't send a serial"
    eis_serial_start = parameters.get("eis-serial", 0)
    if eis_serial_start >= 0:
        global eis_serial
        eis_serial = count(start=eis_serial_start)

    # Zone set number, < 0 means "don't send a zone_set"
    zone_set_start = parameters.get("zone-set", 0)
    if zone_set_start >= 0:
        global zone_set
        zone_set = count(start=zone_set_start)
        mock.current_zone_set = next(zone_set)
    else:
        mock.current_zone_set = None

    # An all-zeroes zone means "don't send a zone"
    mock.current_zones = parameters.get("zones", ((1920, 1080, 0, 0),))
    if mock.current_zones[0] == (0, 0, 0, 0):
        mock.current_zones = None

    # second set of zones after the change signal
    mock.changed_zones = parameters.get("changed-zones", ((0, 0, 0, 0),))
    if mock.changed_zones[0] == (0, 0, 0, 0):
        mock.changed_zones = None

    # milliseconds until the zones change to the changed_zones
    mock.change_zones_after = parameters.get("change-zones-after", 0)

    # List of barrier ids to fail
    mock.failed_barriers = parameters.get("failed-barriers", [])

    # When to send the Activated signal (in ms after Enable), 0 means no
    # signal
    mock.activated_after = parameters.get("activated-after", 0)

    # Barrier ID that triggers Activated (-1 means don't add barrier id)
    mock.activated_barrier = parameters.get("activated-barrier", None)

    # Position tuple for Activated signal, None means don't add position
    mock.activated_position = parameters.get("activated-position", None)

    # When to send the Deactivated signal (in ms after Activated), 0 means no
    # signal
    mock.deactivated_after = parameters.get("deactivated-after", 0)

    # Position tuple for Deactivated signal, None means don't add position
    mock.deactivated_position = parameters.get("deactivated-position", None)

    # When to send the Disabled signal (in ms after Enabled), 0 means no
    # signal
    mock.disabled_after = parameters.get("disabled-after", 0)

    # How many ms to signal Session.Closed after Start
    mock.close_after_enable = parameters.get("close-after-enable", 0)

    mock.AddProperties(
        MAIN_IFACE,
        dbus.Dictionary(
            {
                "version": dbus.UInt32(parameters.get("version", 1)),
                "SupportedCapabilities": dbus.UInt32(
                    parameters.get("capabilities", 0xF)
                ),
            }
        ),
    )

    mock.active_sessions: Dict[str, Session] = {}


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="sa{sv}",
    out_signature="o",
)
def CreateSession(self, parent_window: str, options: ASVType, sender: str):
    try:
        request = Request(bus_name=self.bus_name, sender=sender, options=options)
        session = Session(bus_name=self.bus_name, sender=sender, options=options)

        response = Response(
            0,
            {
                "capabilities": dbus.UInt32(0xF, variant_level=1),
                "session_handle": dbus.ObjectPath(session.handle),
            },
        )
        self.active_sessions[session.handle] = session

        logger.debug(f"CreateSession with response {response}")
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
def ConnectToEIS(self, session_handle: str, options: ASVType, sender: str):
    try:
        import socket

        sockets = socket.socketpair()
        # Write some random data down so it'll break anything that actually
        # expects the socket to be a real EIS socket
        sockets[0].send(b"VANILLA")
        fd = sockets[1]
        logger.debug(f"ConnectToEIS with fd {fd.fileno()}")
        return dbus.types.UnixFd(fd)
    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="oa{sv}",
    out_signature="o",
)
def GetZones(self, session_handle: str, options: ASVType, sender: str):
    try:
        request = Request(bus_name=self.bus_name, sender=sender, options=options)

        if session_handle not in self.active_sessions:
            request.respond(Response(2, {}, delay=self.delay))
            return request.handle

        zone_set = self.current_zone_set
        zones = self.current_zones

        results = {}
        if zone_set is not None:
            results["zone_set"] = dbus.UInt32(zone_set, variant_level=1)
        if zones is not None:
            results["zones"] = dbus.Array(
                [dbus.Struct(z, signature="uuii") for z in zones],
                signature="(uuii)",
                variant_level=1,
            )

        response = Response(response=0, results=results)

        logger.debug(f"GetZones with response {response}")
        request.respond(response, delay=self.delay)

        if self.change_zones_after > 0:

            def change_zones():
                global zone_set

                logger.debug("Changing Zones")
                opts = {"zone_set": dbus.UInt32(self.current_zone_set, variant_level=1)}
                self.current_zone_set = next(zone_set)
                self.current_zones = self.changed_zones
                self.EmitSignalDetailed(
                    "",
                    "ZonesChanged",
                    "oa{sv}",
                    [dbus.ObjectPath(session_handle), opts],
                    details={"destination": sender},
                )

            GLib.timeout_add(self.change_zones_after, change_zones)

            self.change_zones_after = 0  # Zones only change once

        return request.handle
    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="oa{sv}aa{sv}u",
    out_signature="o",
)
def SetPointerBarriers(
    self,
    session_handle: str,
    options: ASVType,
    barriers: List[ASVType],
    zone_set: int,
    sender: str,
):
    try:
        request = Request(bus_name=self.bus_name, sender=sender, options=options)

        if (
            session_handle not in self.active_sessions
            or zone_set != self.current_zone_set
        ):
            response = Response(2, {})
        else:
            results = {
                "failed_barriers": dbus.Array(
                    self.failed_barriers, signature="u", variant_level=1
                )
            }
            response = Response(0, results)

        logger.debug(f"SetPointerBarriers with response {response}")
        request.respond(response, delay=self.delay)

        return request.handle
    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="oa{sv}",
    out_signature="",
)
def Enable(self, session_handle, options, sender):
    try:
        logger.debug(f"Enable with options {options}")
        allowed_options = []

        if not all([k in allowed_options for k in options]):
            logger.error("Enable does not support options")

        if self.activated_after > 0:
            current_eis_serial = next(eis_serial) if eis_serial else None

            def send_activated():
                opts = {}
                if current_eis_serial is not None:
                    opts["activation_id"] = dbus.UInt32(
                        current_eis_serial, variant_level=1
                    )

                if self.activated_position is not None:
                    opts["cursor_position"] = dbus.Struct(
                        self.activated_position, signature="dd", variant_level=1
                    )
                if self.activated_barrier is not None:
                    opts["barrier_id"] = dbus.UInt32(
                        self.activated_barrier, variant_level=1
                    )

                self.EmitSignalDetailed(
                    "",
                    "Activated",
                    "oa{sv}",
                    [dbus.ObjectPath(session_handle), opts],
                    details={"destination": sender},
                )

            GLib.timeout_add(self.activated_after, send_activated)

            if self.deactivated_after > 0:

                def send_deactivated():
                    opts = {}
                    if current_eis_serial:
                        opts["activation_id"] = dbus.UInt32(
                            current_eis_serial, variant_level=1
                        )

                    if self.deactivated_position is not None:
                        opts["cursor_position"] = dbus.Struct(
                            self.deactivated_position, signature="dd", variant_level=1
                        )

                    self.EmitSignalDetailed(
                        "",
                        "Deactivated",
                        "oa{sv}",
                        [dbus.ObjectPath(session_handle), opts],
                        details={"destination": sender},
                    )

                GLib.timeout_add(
                    self.activated_after + self.deactivated_after, send_deactivated
                )

        if self.disabled_after > 0:

            def send_disabled():
                self.EmitSignalDetailed(
                    "",
                    "Disabled",
                    "oa{sv}",
                    [dbus.ObjectPath(session_handle), {}],
                    details={"destination": sender},
                )

            GLib.timeout_add(self.disabled_after, send_disabled)

        if self.close_after_enable > 0:
            session = self.active_sessions[session_handle]
            session.close({}, self.close_after_enable)

    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="oa{sv}",
    out_signature="",
)
def Disable(self, session_handle, options, sender):
    try:
        logger.debug(f"Disable with options {options}")
        allowed_options = []

        if not all([k in allowed_options for k in options]):
            logger.error("Disable does not support options")
    except Exception as e:
        logger.critical(e)


@dbus.service.method(
    MAIN_IFACE,
    sender_keyword="sender",
    in_signature="oa{sv}",
    out_signature="",
)
def Release(self, session_handle, options, sender):
    try:
        logger.debug(f"Release with options {options}")
        allowed_options = ["cursor_position"]

        if not all([k in allowed_options for k in options]):
            logger.error("Invalid options for Release")
    except Exception as e:
        logger.critical(e)
