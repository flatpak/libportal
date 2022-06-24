# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from . import PortalTest

import dbus
import gi
import logging
import os

from typing import NamedTuple, TextIO

gi.require_version("Xdp", "1.0")
from gi.repository import GLib, Xdp

logger = logging.getLogger(__name__)


class SessionCreationFailed(Exception):
    def __init__(self, glib_error):
        self.glib_error = glib_error

    def __str__(self):
        return f"{type(self).__name__}: {self.glib_error}"


class SessionSetup(NamedTuple):
    session: Xdp.Session
    pw_fd: TextIO


class TestRemoteDesktop(PortalTest):
    def test_version(self):
        self.assert_version_eq(1)

    def short_mainloop(self):
        """
        Run a short iteration of our mainloop. Useful to for pure method calls
        that still require dbusmock etc. to update after the invocation.
        """
        GLib.timeout_add(200, self.mainloop.quit)
        self.mainloop.run()

    def create_session(
        self,
        params=None,
        parent=None,
        cancellable=None,
        devices=Xdp.DeviceType.POINTER
        | Xdp.DeviceType.KEYBOARD
        | Xdp.DeviceType.TOUCHSCREEN,
        outputs=Xdp.OutputType.NONE,
        flags=Xdp.RemoteDesktopFlags.NONE,
        cursor_mode=Xdp.CursorMode.HIDDEN,
        start_session=True,
    ) -> SessionSetup:
        params = params or {}
        # To make the tests easier, load ScreenCast automatically if we have
        # any outputs specified.
        extra_templates = []
        if outputs:
            extra_templates = [("ScreenCast", {})]
        self.setup_daemon(params=params, extra_templates=extra_templates)

        xdp = Xdp.Portal.new()
        assert xdp is not None

        session, session_error = None, None
        create_session_done_invoked = False

        def create_session_done(portal, task, data):
            nonlocal session, session_error
            nonlocal create_session_done_invoked

            create_session_done_invoked = True

            try:
                session = portal.create_remote_desktop_session_finish(task)
            except GLib.GError as e:
                session_error = e
            self.mainloop.quit()

        xdp.create_remote_desktop_session(
            devices=devices,
            outputs=outputs,
            flags=flags,
            cursor_mode=cursor_mode,
            cancellable=cancellable,
            callback=create_session_done,
            data=None,
        )

        self.mainloop.run()
        assert create_session_done_invoked
        if session_error is not None:
            raise SessionCreationFailed(session_error)

        assert session.get_session_type() == Xdp.SessionType.REMOTE_DESKTOP

        if outputs:
            # open the PW remote by default since we need it anyway for Start()
            handle = session.open_pipewire_remote()
            pw_fd = os.fdopen(handle)
            assert pw_fd.read() == "I AM GROO^WPIPEWIRE"
        else:
            pw_fd = None

        if start_session:

            def start_done(portal, task):
                session.start_finish(task)
                self.mainloop.quit()

            session.start(callback=start_done)
            self.mainloop.run()

        return SessionSetup(
            session=session,
            pw_fd=pw_fd,
        )

    def test_create_session(self):
        """
        Create a session with some "random" values and ensure that they're
        passed through to the portal.
        """
        devices = Xdp.DeviceType.POINTER | Xdp.DeviceType.KEYBOARD
        outputs = Xdp.OutputType.MONITOR | Xdp.OutputType.WINDOW
        cursor_mode = Xdp.CursorMode.METADATA
        flags = Xdp.RemoteDesktopFlags.MULTIPLE

        self.create_session(
            devices=devices,
            outputs=outputs,
            flags=flags,
            cursor_mode=cursor_mode,
            start_session=False,
        )

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("CreateSession")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        (options,) = args

        assert list(options.keys()) == [
            "handle_token",
            "session_handle_token",
        ]

        method_calls = self.mock_interface.GetMethodCalls("SelectDevices")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args

        assert list(options.keys()) == [
            "handle_token",
            "types",
        ]
        assert options["types"] == devices

        method_calls = self.mock_interface.GetMethodCalls("SelectSources")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args

        assert list(options.keys()) == [
            "handle_token",
            "types",
            "multiple",
            "cursor_mode",
        ]

        assert options["types"] == outputs
        assert options["multiple"] == flags
        assert options["cursor_mode"] == cursor_mode

    def test_create_session_no_outputs(self):
        """
        Create a session with no output value, ensure it's a "pure"
        remote desktop session.
        """
        devices = Xdp.DeviceType.POINTER | Xdp.DeviceType.KEYBOARD
        outputs = Xdp.OutputType.NONE
        cursor_mode = Xdp.CursorMode.METADATA
        flags = Xdp.RemoteDesktopFlags.MULTIPLE

        self.create_session(
            devices=devices,
            outputs=outputs,
            flags=flags,
            cursor_mode=cursor_mode,
            start_session=False,
        )

        # Now verify our DBus calls were correct
        method_calls = self.mock_interface.GetMethodCalls("CreateSession")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        (options,) = args

        assert list(options.keys()) == [
            "handle_token",
            "session_handle_token",
        ]

        method_calls = self.mock_interface.GetMethodCalls("SelectDevices")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args

        assert list(options.keys()) == [
            "handle_token",
            "types",
        ]
        assert options["types"] == devices

        # No outputs means this should never get called
        method_calls = self.mock_interface.GetMethodCalls("SelectSources")
        assert len(method_calls) == 0

    def test_session_start(self):
        """
        Create and start a session
        """
        devices = Xdp.DeviceType.POINTER | Xdp.DeviceType.KEYBOARD
        outputs = Xdp.OutputType.NONE
        params = {"devices": devices}

        setup = self.create_session(
            params=params,
            devices=devices,
            outputs=outputs,
            start_session=False,
        )

        session = setup.session
        start_done_invoked = False
        start_result, start_error = None, None

        def start_done(portal, task):
            nonlocal start_done_invoked
            nonlocal start_result, start_error

            start_done_invoked = True
            try:
                start_result = session.start_finish(task)
            except GLib.Error as e:
                start_error = e
            self.mainloop.quit()

        session.start(parent=None, cancellable=None, callback=start_done)

        self.mainloop.run()
        assert start_done_invoked
        assert start_error is None
        assert start_result is True

        method_calls = self.mock_interface.GetMethodCalls("Start")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, parent_window, options = args
        assert parent_window == ""
        assert list(options.keys()) == ["handle_token"]

        assert session.get_devices() == devices

        # This is a pure RD session, so all of the SC-specific ones are
        # expected to be NULL-like
        assert session.get_persist_mode() == Xdp.PersistMode.NONE
        assert session.get_streams() is None
        assert session.get_restore_token() is None

    def test_notify_button(self):
        setup = self.create_session()
        session = setup.session

        session.pointer_button(2, Xdp.ButtonState.PRESSED)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyPointerButton")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, button, state = args
        assert button == 2
        assert state == 1

    def test_notify_key(self):
        setup = self.create_session()
        session = setup.session

        session.keyboard_key(False, 3, Xdp.KeyState.PRESSED)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyKeyboardKeycode")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, key, state = args
        assert key == 3
        assert state == 1

        session.keyboard_key(True, 4, Xdp.KeyState.PRESSED)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyKeyboardKeysym")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, key, state = args
        assert key == 4
        assert state == 1

    def test_notify_motion(self):
        setup = self.create_session()
        session = setup.session

        session.pointer_motion(-1.4, -10.1)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyPointerMotion")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, x, y = args
        assert (x, y) == (-1.4, -10.1)

    def test_notify_motion_absolute(self):
        setup = self.create_session()
        session = setup.session

        session.pointer_position(123, -1.3, -5.2)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyPointerMotionAbsolute")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, stream, x, y = args
        assert stream == 123
        assert (x, y) == (-1.3, -5.2)

    def test_notify_axis(self):
        setup = self.create_session()
        session = setup.session

        session.pointer_axis(False, -3.3, -8.2)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyPointerAxis")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, x, y = args
        assert "finish" in options
        assert not options["finish"]
        assert (x, y) == (-3.3, -8.2)

    def test_notify_axis_discrete(self):
        setup = self.create_session()
        session = setup.session

        session.pointer_axis_discrete(Xdp.DiscreteAxis.HORIZONTAL_SCROLL, 4)
        session.pointer_axis_discrete(Xdp.DiscreteAxis.VERTICAL_SCROLL, 6)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyPointerAxisDiscrete")
        assert len(method_calls) == 2
        _, args = method_calls.pop(0)
        session_handle, options, axis, steps = args
        assert axis == Xdp.DiscreteAxis.HORIZONTAL_SCROLL
        assert steps == 4

        _, args = method_calls.pop(0)
        session_handle, options, axis, steps = args
        assert axis == Xdp.DiscreteAxis.VERTICAL_SCROLL
        assert steps == 6

    def test_notify_touch(self):
        setup = self.create_session()
        session = setup.session

        session.touch_down(2, 3, 4.4, 5.5)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyTouchDown")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, stream, slot, x, y = args
        assert (stream, slot, x, y) == (2, 3, 4.4, 5.5)

        session.touch_position(6, 7, 8.8, 9.9)  # libportal doesn't check touch state
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyTouchMotion")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, stream, slot, x, y = args
        assert (stream, slot, x, y) == (6, 7, 8.8, 9.9)

        session.touch_up(10)
        self.short_mainloop()

        method_calls = self.mock_interface.GetMethodCalls("NotifyTouchUp")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options, slot = args
        assert slot == 10
