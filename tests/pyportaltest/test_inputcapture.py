# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from . import PortalTest
from typing import List, Optional

import gi
import logging
import pytest
import os

gi.require_version("Xdp", "1.0")
from gi.repository import GLib, Gio, Xdp

logger = logging.getLogger(f"test.{__name__}")
logger.setLevel(logging.DEBUG)


class SessionSetup:
    def __init__(
        self,
        session: Xdp.InputCaptureSession = None,
        zones: Optional[List[Xdp.InputCaptureZone]] = None,
        barriers: Optional[List[Xdp.InputCapturePointerBarrier]] = None,
        failed_barriers: Optional[List[Xdp.InputCapturePointerBarrier]] = None,
        session_handle_token: Optional[str] = None,
    ):
        self.session = session
        self.zones = zones or []
        self.barriers = barriers or []
        self.failed_barriers = failed_barriers or []
        self.session_handle_token = session_handle_token


class SessionCreationFailed(Exception):
    def __init__(self, glib_error):
        self.glib_error = glib_error

    def __str__(self):
        return f"SessionCreationFailed: {self.glib_error}"


class TestInputCapture(PortalTest):
    def create_session_with_barriers(
        self,
        params=None,
        parent=None,
        capabilities=Xdp.InputCapability.POINTER,
        barriers=None,
        allow_failed_barriers=False,
        cancellable=None,
    ) -> SessionSetup:
        """
        Session creation helper. This function creates a session and sets up
        pointer barriers, with defaults for everything.
        """
        params = params or {}
        self.setup_daemon(params)

        xdp = Xdp.Portal.new()
        assert xdp is not None

        session, session_error = None, None
        create_session_done_invoked = False

        def create_session_done(portal, task, data):
            nonlocal session, session_error
            nonlocal create_session_done_invoked

            create_session_done_invoked = True
            try:
                session = portal.create_input_capture_session_finish(task)
                if session is None:
                    session_error = Exception("XdpSession is NULL")
            except GLib.GError as e:
                session_error = e
            self.mainloop.quit()

        xdp.create_input_capture_session(
            parent=parent,
            capabilities=capabilities,
            cancellable=cancellable,
            callback=create_session_done,
            data=None,
        )

        self.mainloop.run()
        assert create_session_done_invoked
        if session_error is not None:
            raise SessionCreationFailed(session_error)
        assert session is not None
        assert session.get_session().get_session_type() == Xdp.SessionType.INPUT_CAPTURE

        # Extract our expected session id. This isn't available from
        # XdpSession so we need to go around it. We can't easily get the
        # sender id so the full path is hard. Let's just extract the token and
        # pretend that's good enough.
        method_calls = self.mock_interface.GetMethodCalls("CreateSession")
        assert len(method_calls) >= 1
        _, args = method_calls.pop()  # Assume the latest has our session
        (_, options) = args
        session_handle = options["session_handle_token"]

        zones = session.get_zones()

        if barriers is None:
            barriers = [Xdp.InputCapturePointerBarrier(id=1, x1=0, x2=1920, y1=0, y2=0)]

        # Check that we get the notify:is-active for each barrier
        active_barriers = []
        inactive_barriers = []

        def notify_active_cb(barrier, pspec):
            nonlocal active_barriers, inactive_barriers

            if barrier.props.is_active:
                active_barriers.append(barrier)
            else:
                inactive_barriers.append(barrier)

        for b in barriers:
            b.connect("notify::is-active", notify_active_cb)

        failed_barriers = None

        def set_pointer_barriers_done(session, task, data):
            nonlocal session_error, failed_barriers
            nonlocal set_pointer_barriers_done_invoked

            set_pointer_barriers_done_invoked = True
            try:
                failed_barriers = session.set_pointer_barriers_finish(task)
            except GLib.GError as e:
                session_error = e
            self.mainloop.quit()

        set_pointer_barriers_done_invoked = False
        session.set_pointer_barriers(
            barriers=barriers,
            cancellable=None,
            callback=set_pointer_barriers_done,
            data=None,
        )
        self.mainloop.run()

        if session_error is not None:
            raise SessionCreationFailed(session_error)

        assert set_pointer_barriers_done_invoked
        assert sorted(active_barriers + inactive_barriers) == sorted(barriers)

        if not allow_failed_barriers:
            assert (
                failed_barriers == []
            ), "Barriers failed but allow_failed_barriers was not set"

        return SessionSetup(
            session=session,
            zones=zones,
            barriers=active_barriers,
            failed_barriers=failed_barriers,
            session_handle_token=session_handle,
        )

    def test_version(self):
        """This tests the test suite setup rather than libportal"""
        params = {}
        self.setup_daemon(params)
        assert self.properties_interface.Get(self.INTERFACE_NAME, "version") == 1

    def test_session_create(self):
        """
        The basic test of successful create and zone check
        """
        params = {
            "zones": [(1920, 1080, 0, 0), (1080, 1920, 1920, 1080)],
            "zone-set": 1234,
        }
        self.setup_daemon(params)

        capabilities = Xdp.InputCapability.POINTER | Xdp.InputCapability.KEYBOARD

        setup = self.create_session_with_barriers(params, capabilities=capabilities)
        assert setup.session is not None
        zones = setup.zones
        assert len(zones) == 2
        z1 = zones[0]
        assert z1.props.width == 1920
        assert z1.props.height == 1080
        assert z1.props.x == 0
        assert z1.props.y == 0
        assert z1.props.zone_set == 1234

        z2 = zones[1]
        assert z2.props.width == 1080
        assert z2.props.height == 1920
        assert z2.props.x == 1920
        assert z2.props.y == 1080
        assert z2.props.zone_set == 1234

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("CreateSession")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        parent, options = args
        assert list(options.keys()) == [
            "handle_token",
            "session_handle_token",
            "capabilities",
        ]
        assert options["capabilities"] == capabilities

        method_calls = self.mock_interface.GetMethodCalls("GetZones")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args
        assert list(options.keys()) == ["handle_token"]

    def test_session_create_cancel_during_create(self):
        """
        Create a session but cancel while waiting for the CreateSession request
        """
        params = {"delay": 1000}
        self.setup_daemon(params)
        cancellable = Gio.Cancellable()
        GLib.timeout_add(300, cancellable.cancel)

        with pytest.raises(SessionCreationFailed) as e:
            self.create_session_with_barriers(params=params, cancellable=cancellable)
            assert "Operation was cancelled" in e.glib_error.message

    def test_session_create_cancel_during_getzones(self):
        """
        Create a session but cancel while waiting for the GetZones request
        """
        # libportal issues two requests: CreateSession and GetZones,
        # param is set for each to delay 500 ms so if we cancel after 700, the
        # one that is cancelled should be the GetZones one.
        # Can't guarantee it but this is the best we can do
        params = {"delay": 500}
        self.setup_daemon(params)
        cancellable = Gio.Cancellable()
        GLib.timeout_add(700, cancellable.cancel)

        with pytest.raises(SessionCreationFailed) as e:
            self.create_session_with_barriers(params=params, cancellable=cancellable)
            assert "Operation was cancelled" in e.glib_error.message

    def test_session_create_no_serial_on_getzones(self):
        """
        Test buggy portal implementation not replying with a zone_set in
        GetZones
        """
        params = {
            "zone-set": -1,
        }

        self.setup_daemon(params)
        with pytest.raises(SessionCreationFailed):
            self.create_session_with_barriers(params)

    def test_session_create_no_zones_on_getzones(self):
        """
        Test buggy portal implementation not replying with a zone
        GetZones
        """
        params = {
            "zones": [(0, 0, 0, 0)],
        }

        self.setup_daemon(params)
        with pytest.raises(SessionCreationFailed):
            self.create_session_with_barriers(params)

    def _test_session_create_without_subref(self):
        """
        Create a new InputCapture session but never access the actual
        input capture session.
        """
        self.setup_daemon({})

        xdp = Xdp.Portal.new()
        assert xdp is not None

        parent_session, session_error = None, None
        create_session_done_invoked = False

        def create_session_done(portal, task, data):
            nonlocal parent_session, session_error
            nonlocal create_session_done_invoked

            create_session_done_invoked = True
            try:
                parent_session = portal.create_input_capture_session_finish(task)
                if parent_session is None:
                    session_error = Exception("XdpSession is NULL")
            except GLib.GError as e:
                session_error = e
            self.mainloop.quit()

        capabilities = Xdp.InputCapability.POINTER | Xdp.InputCapability.KEYBOARD
        xdp.create_input_capture_session(
            parent=None,
            capabilities=capabilities,
            cancellable=None,
            callback=create_session_done,
            data=None,
        )

        self.mainloop.run()
        assert create_session_done_invoked

        # Explicitly don't call parent_session.get_input_capture_session()
        # since that would cause python to g_object_ref the IC session.
        # By not doing so we never ref that object and can test for the correct
        # cleanup

    def test_connect_to_eis(self):
        """
        The basic test of retrieving the EIS handle
        """
        params = {}
        self.setup_daemon(params)
        setup = self.create_session_with_barriers(params)
        assert setup.session is not None

        handle = setup.session.connect_to_eis()
        assert handle >= 0

        fd = os.fdopen(handle)
        buf = fd.read()
        assert buf == "VANILLA"  # template sends this by default

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("ConnectToEIS")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        parent, options = args
        assert "handle_token" not in options  # This is not a Request
        assert list(options.keys()) == []

    def test_pointer_barriers_success(self):
        """
        Some successful pointer barriers
        """
        b1 = Xdp.InputCapturePointerBarrier(id=1, x1=0, x2=1920, y1=0, y2=0)
        b2 = Xdp.InputCapturePointerBarrier(id=2, x1=1920, x2=1920, y1=0, y2=1080)

        params = {}
        self.setup_daemon(params)
        setup = self.create_session_with_barriers(params, barriers=[b1, b2])
        assert setup.barriers == [b1, b2]

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("SetPointerBarriers")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, barriers, zone_set = args
        assert list(options.keys()) == ["handle_token"]
        for b in barriers:
            assert "barrier_id" in b
            assert "position" in b
            assert b["barrier_id"] in [1, 2]
            x1, y1, x2, y2 = [int(x) for x in b["position"]]
            if b["barrier_id"] == 1:
                assert (x1, y1, x2, y2) == (0, 0, 1920, 0)
            if b["barrier_id"] == 2:
                assert (x1, y1, x2, y2) == (1920, 0, 1920, 1080)

    def test_pointer_barriers_failures(self):
        """
        Test with some barriers failing
        """
        b1 = Xdp.InputCapturePointerBarrier(id=1, x1=0, x2=1920, y1=0, y2=0)
        b2 = Xdp.InputCapturePointerBarrier(id=2, x1=1, x2=2, y1=3, y2=4)
        b3 = Xdp.InputCapturePointerBarrier(id=3, x1=1, x2=2, y1=3, y2=4)
        b4 = Xdp.InputCapturePointerBarrier(id=4, x1=1920, x2=1920, y1=0, y2=1080)

        params = {"failed-barriers": [2, 3]}
        self.setup_daemon(params)
        setup = self.create_session_with_barriers(
            params, barriers=[b1, b2, b3, b4], allow_failed_barriers=True
        )
        assert setup.barriers == [b1, b4]
        assert setup.failed_barriers == [b2, b3]

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("SetPointerBarriers")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, barriers, zone_set = args
        assert list(options.keys()) == ["handle_token"]
        for b in barriers:
            assert "barrier_id" in b
            assert "position" in b
            assert b["barrier_id"] in [1, 2, 3, 4]
            x1, y1, x2, y2 = [int(x) for x in b["position"]]
            if b["barrier_id"] == 1:
                assert (x1, y1, x2, y2) == (0, 0, 1920, 0)
            if b["barrier_id"] in [2, 3]:
                assert (x1, y1, x2, y2) == (1, 3, 2, 4)
            if b["barrier_id"] == 4:
                assert (x1, y1, x2, y2) == (1920, 0, 1920, 1080)

    def test_enable_disable_release(self):
        """
        Test enable/disable calls
        """
        params = {}
        self.setup_daemon(params)

        setup = self.create_session_with_barriers(params)
        session = setup.session

        session.enable()
        session.disable()
        session.release(activation_id=456)  # fake id, doesn't matter here

        self.mainloop.run()

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("Enable")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args
        assert list(options.keys()) == []

        method_calls = self.mock_interface.GetMethodCalls("Disable")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args
        assert list(options.keys()) == []

        method_calls = self.mock_interface.GetMethodCalls("Release")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args
        assert list(options.keys()) == ["activation_id"]

    def test_release_at(self):
        """
        Test the release_at call with a cursor position
        """
        params = {}
        self.setup_daemon(params)

        setup = self.create_session_with_barriers(params)
        session = setup.session

        # libportal allows us to call Release without Enable first,
        # we just fake an activation_id
        session.release_at(
            activation_id=456, cursor_x_position=10, cursor_y_position=10
        )
        self.mainloop.run()

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("Release")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args
        assert list(options.keys()) == ["activation_id", "cursor_position"]
        cursor_position = options["cursor_position"]
        assert cursor_position == (10.0, 10.0)

    def test_activated(self):
        """
        Test the Activated signal
        """
        params = {
            "eis-serial": 123,
            "activated-after": 20,
            "activated-barrier": 1,
            "activated-position": (10.0, 20.0),
            "deactivated-after": 20,
            "deactivated-position": (20.0, 30.0),
        }
        self.setup_daemon(params)

        setup = self.create_session_with_barriers(params)
        session = setup.session

        session_activated_signal_received = False
        session_deactivated_signal_received = False
        signal_activated_options = None
        signal_deactivated_options = None
        signal_activation_id = None
        signal_deactivation_id = None

        def session_activated(session, activation_id, opts):
            nonlocal session_activated_signal_received
            nonlocal signal_activation_id, signal_activated_options
            session_activated_signal_received = True
            signal_activated_options = opts
            signal_activation_id = activation_id

        def session_deactivated(session, activation_id, opts):
            nonlocal session_deactivated_signal_received
            nonlocal signal_deactivation_id, signal_deactivated_options
            session_deactivated_signal_received = True
            signal_deactivated_options = opts
            signal_deactivation_id = activation_id
            self.mainloop.quit()

        session.connect("activated", session_activated)
        session.connect("deactivated", session_deactivated)
        session.enable()

        self.mainloop.run()

        assert session_activated_signal_received
        assert signal_activated_options is not None
        assert signal_activation_id == 123
        assert list(signal_activated_options.keys()) == [
            "activation_id",
            "cursor_position",
            "barrier_id",
        ]
        assert signal_activated_options["barrier_id"] == 1
        assert signal_activated_options["cursor_position"] == (10.0, 20.0)
        assert signal_activated_options["activation_id"] == 123

        assert session_deactivated_signal_received
        assert signal_deactivated_options is not None
        assert signal_deactivation_id == 123
        assert list(signal_deactivated_options.keys()) == [
            "activation_id",
            "cursor_position",
        ]
        assert signal_deactivated_options["cursor_position"] == (20.0, 30.0)
        assert signal_deactivated_options["activation_id"] == 123

    def test_zones_changed(self):
        """
        Test the ZonesChanged signal
        """
        params = {
            "zones": [(1920, 1080, 0, 0), (1080, 1920, 1920, 1080)],
            "changed-zones": [(1024, 768, 0, 0)],
            "change-zones-after": 200,
            "zone-set": 567,
        }
        self.setup_daemon(params)

        setup = self.create_session_with_barriers(params)
        session = setup.session

        signal_received = False
        signal_options = None
        zone_props = {z: None for z in setup.zones}

        def zones_changed(session, opts):
            nonlocal signal_received, signal_options, zone_props
            signal_received = True
            signal_options = opts
            if signal_received and all([v == False for v in zone_props.values()]):
                self.mainloop.quit()

        session.connect("zones-changed", zones_changed)

        def zones_is_valid_changed(zone, pspec):
            nonlocal zone_props, signal_received
            zone_props[zone] = zone.props.is_valid
            if signal_received and all([v == False for v in zone_props.values()]):
                self.mainloop.quit()

        for z in setup.zones:
            z.connect("notify::is-valid", zones_is_valid_changed)

        self.mainloop.run()

        assert signal_received
        assert signal_options is not None
        assert list(signal_options.keys()) == ["zone_set"]
        assert signal_options["zone_set"] == 567

        assert all([z.props.zone_set == 568 for z in session.get_zones()])
        assert all([v == False for v in zone_props.values()])

    def test_disabled(self):
        """
        Test the Disabled signal
        """
        params = {
            "disabled-after": 20,
        }
        self.setup_daemon(params)

        setup = self.create_session_with_barriers(params)
        session = setup.session

        disabled_signal_received = False

        def session_disabled(session, options):
            nonlocal disabled_signal_received
            disabled_signal_received = True
            self.mainloop.quit()

        session.connect("disabled", session_disabled)

        session.enable()

        self.mainloop.run()

        assert disabled_signal_received

    def test_close_session(self):
        """
        Ensure that closing our session explicitly closes the session on DBus.
        """
        setup = self.create_session_with_barriers()
        session = setup.session
        xdp_session = setup.session.get_session()

        was_closed = False

        def method_called(method_name, method_args, path):
            nonlocal was_closed

            if method_name == "Close" and path.endswith(setup.session_handle_token):
                was_closed = True
                self.mainloop.quit()

        bus = self.get_dbus()
        bus.add_signal_receiver(
            handler_function=method_called,
            signal_name="MethodCalled",
            dbus_interface="org.freedesktop.DBus.Mock",
            path_keyword="path",
        )

        xdp_session.close()
        self.mainloop.run()

        assert was_closed is True

    def test_close_session_signal(self):
        """
        Ensure that we get the GObject signal when our session is closed
        externally.
        """
        params = {"close-after-enable": 500}
        setup = self.create_session_with_barriers(params)
        session = setup.session
        xdp_session = setup.session.get_session()

        session_closed_signal_received = False

        def session_closed(session):
            nonlocal session_closed_signal_received
            session_closed_signal_received = True

        xdp_session.connect("closed", session_closed)

        session.enable()
        self.mainloop.run()

        assert session_closed_signal_received is True
