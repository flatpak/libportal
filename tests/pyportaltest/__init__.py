# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from typing import Any, Dict, List, Tuple

import gi
from gi.repository import GLib
from dbus.mainloop.glib import DBusGMainLoop

import dbus
import dbusmock
import fcntl
import logging
import os
import pytest
import subprocess

logging.basicConfig(format="%(levelname)s | %(name)s: %(message)s", level=logging.DEBUG)
logger = logging.getLogger("pyportaltest")

DBusGMainLoop(set_as_default=True)

# Uncomment this to have dbus-monitor listen on the normal session address
# rather than the test DBus. This can be useful for cases where *something*
# messes up and tests run against the wrong bus.
#
# session_dbus_address = os.environ["DBUS_SESSION_BUS_ADDRESS"]


def start_dbus_monitor() -> "subprocess.Process":
    import subprocess

    env = os.environ.copy()
    try:
        env["DBUS_SESSION_BUS_ADDRESS"] = session_dbus_address
    except NameError:
        # See comment above
        pass

    argv = ["dbus-monitor", "--session"]
    mon = subprocess.Popen(argv, env=env)

    def stop_dbus_monitor():
        mon.terminate()
        mon.wait()

    GLib.timeout_add(2000, stop_dbus_monitor)
    return mon


class PortalTest(dbusmock.DBusTestCase):
    """
    Parent class for portal tests. Subclass from this and name it after the
    portal, e.g. ``TestWallpaper``.

    .. attribute:: portal_interface

        The :class:`dbus.Interface` referring to our portal

    .. attribute:: properties_interface

        A convenience :class:`dbus.Interface` referring to the DBus Properties
        interface, call ``Get``, ``Set`` or ``GetAll`` on this interface to
        retrieve the matching property/properties.

    .. attribute:: mock_interface

        The DBusMock :class:`dbus.Interface` that controls our DBus
        appearance.

    """
    @classmethod
    def setUpClass(cls):
        if cls.__name__ != "PortalTest":
            cls.PORTAL_NAME = cls.__name__.removeprefix("Test")
            cls.INTERFACE_NAME = f"org.freedesktop.portal.{cls.PORTAL_NAME}"
        os.environ["LIBPORTAL_TEST_SUITE"] = "1"

        try:
            dbusmock.mockobject.DBusMockObject.EmitSignalDetailed
        except AttributeError:
            pytest.skip("Updated version of dbusmock required")

    def setUp(self):
        self.p_mock = None
        self._mainloop = None
        self.dbus_monitor = None

    def setup_daemon(self, params=None):
        """
        Start a DBusMock daemon in a separate process
        """
        self.start_session_bus()
        self.p_mock, self.obj_portal = self.spawn_server_template(
            template=f"pyportaltest/templates/{self.PORTAL_NAME.lower()}.py",
            parameters=params,
            stdout=subprocess.PIPE,
        )
        flags = fcntl.fcntl(self.p_mock.stdout, fcntl.F_GETFL)
        fcntl.fcntl(self.p_mock.stdout, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        self.mock_interface = dbus.Interface(self.obj_portal, dbusmock.MOCK_IFACE)
        self.properties_interface = dbus.Interface(
            self.obj_portal, dbus.PROPERTIES_IFACE
        )
        self.portal_interface = dbus.Interface(self.obj_portal, self.INTERFACE_NAME)

        self.dbus_monitor = start_dbus_monitor()

    def tearDown(self):
        if self.p_mock:
            if self.p_mock.stdout:
                out = (self.p_mock.stdout.read() or b"").decode("utf-8")
                if out:
                    print(out)
                self.p_mock.stdout.close()
            self.p_mock.terminate()
            self.p_mock.wait()

        if self.dbus_monitor:
            self.dbus_monitor.terminate()
            self.dbus_monitor.wait()

    @property
    def mainloop(self):
        """
        The mainloop for this test. This mainloop automatically quits after a
        fixed timeout, but only on the first run. That's usually enough for
        tests, if you need to call mainloop.run() repeatedly ensure that a
        timeout handler is set to ensure quick test case failure  in case of
        error.
        """
        if self._mainloop is None:

            def quit():
                self._mainloop.quit()
                self._mainloop = None

            self._mainloop = GLib.MainLoop()
            GLib.timeout_add(2000, quit)

        return self._mainloop

    def assert_version_eq(self, version: int):
        """Assert the given version number is the one our portal exports"""
        interface_name = self.INTERFACE_NAME
        params = {}
        self.setup_daemon(params)
        assert self.properties_interface.Get(interface_name, "version") == version
