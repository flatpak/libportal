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
    session_handle_token: str
    pw_fd: TextIO


class TestScreenCast(PortalTest):
    def test_version(self):
        self.assert_version_eq(4)

    def create_session(
        self,
        params=None,
        parent=None,
        cancellable=None,
        outputs=Xdp.OutputType.MONITOR,
        flags=Xdp.ScreencastFlags.NONE,
        cursor_mode=Xdp.CursorMode.HIDDEN,
        persist_mode=Xdp.PersistMode.NONE,
        restore_token=None,
    ) -> SessionSetup:
        self.setup_daemon(params or {})

        xdp = Xdp.Portal.new()
        assert xdp is not None

        session, session_error = None, None
        create_session_done_invoked = False

        def create_session_done(portal, task, data):
            nonlocal session, session_error
            nonlocal create_session_done_invoked

            create_session_done_invoked = True

            try:
                session = portal.create_screencast_session_finish(task)
            except GLib.GError as e:
                session_error = e
            self.mainloop.quit()

        xdp.create_screencast_session(
            outputs=outputs,
            flags=flags,
            cursor_mode=cursor_mode,
            persist_mode=persist_mode,
            restore_token=restore_token,
            cancellable=cancellable,
            callback=create_session_done,
            data=None,
        )

        self.mainloop.run()
        assert create_session_done_invoked
        if session_error is not None:
            raise SessionCreationFailed(session_error)

        # Extract our expected session id. This isn't available from
        # XdpSession so we need to go around it. We can't easily get the
        # sender id so the full path is hard. Let's just extract the token and
        # pretend that's good enough.
        method_calls = self.mock_interface.GetMethodCalls("CreateSession")
        assert len(method_calls) >= 1
        _, args = method_calls.pop()  # Assume the latest has our session
        (options,) = args
        session_handle = options["session_handle_token"]

        assert session.get_session_type() == Xdp.SessionType.SCREENCAST

        # open the PW remote by default since we need it anyway for Start()
        handle = session.open_pipewire_remote()
        pw_fd = os.fdopen(handle)
        assert pw_fd.read() == "I AM GROO^WPIPEWIRE"

        return SessionSetup(
            session=session,
            session_handle_token=session_handle,
            pw_fd=pw_fd,
        )

    def test_create_session(self):
        """
        Create a session with some "random" values and ensure that they're
        passed through to the portal.
        """
        outputs = Xdp.OutputType.MONITOR | Xdp.OutputType.WINDOW
        cursor_mode = Xdp.CursorMode.METADATA
        persist_mode = Xdp.PersistMode.PERSISTENT
        restore_token = "12345"
        flags = Xdp.ScreencastFlags.MULTIPLE

        self.create_session(
            outputs=outputs,
            flags=flags,
            cursor_mode=cursor_mode,
            restore_token=restore_token,
            persist_mode=persist_mode,
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

        # This method doesn't exist on the interface anyway but since we share
        # code paths with RemoteDesktop let's ensure we didn't event try to
        # call it.
        method_calls = self.mock_interface.GetMethodCalls("SelectDevices")
        assert len(method_calls) == 0

        method_calls = self.mock_interface.GetMethodCalls("SelectSources")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args

        assert list(options.keys()) == [
            "handle_token",
            "types",
            "multiple",
            "cursor_mode",
            "persist_mode",
            "restore_token",
        ]

        assert options["types"] == outputs
        assert options["multiple"] == flags
        assert options["cursor_mode"] == cursor_mode
        assert options["restore_token"] == restore_token
        assert options["persist_mode"] == persist_mode

        method_calls = self.mock_interface.GetMethodCalls("OpenPipeWireRemote")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args
        assert list(options.keys()) == []

    def test_create_session_v3(self):
        """
        persist_mode and restore_token were added in v4 of the interface, must
        not be part of SelectSources
        """
        params = {"version": 3}

        persist_mode = Xdp.PersistMode.PERSISTENT
        restore_token = "12345"
        self.create_session(
            params=params,
            restore_token=restore_token,
            persist_mode=persist_mode,
        )

        method_calls = self.mock_interface.GetMethodCalls("SelectSources")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, options = args

        assert "persist_mode" not in options.keys()
        assert "restore_token" not in options.keys()

    def test_session_restore_token_null_if_not_started(self):
        """
        Passed in a restore token but unless the session is started it
        should be NULL.
        """
        restore_token = "12345"
        persist_mode = Xdp.PersistMode.TRANSIENT
        params = {
            "persist-mode": persist_mode,
        }
        setup = self.create_session(
            params=params, restore_token=restore_token, persist_mode=persist_mode
        )

        session = setup.session
        assert session.get_restore_token() is None

    def test_session_start(self):
        """
        Create and start a session
        """

        s = [
            {
                "position": (1, 2),
                "size": (32, 43),
                "source_type": dbus.UInt32(Xdp.OutputType.WINDOW),
            },
            {
                "position": (3, 4),
                "size": (54, 65),
                "source_type": dbus.UInt32(Xdp.OutputType.MONITOR),
            },
        ]

        streams = dbus.Array(
            [(dbus.UInt32(idx), stream) for idx, stream in enumerate(s)],
            signature="(ua{sv})",
            variant_level=1,
        )
        params = {
            "streams": streams,
            "persist-mode": Xdp.PersistMode.TRANSIENT,
        }

        restore_token = "foo"  # ignored since we're not continuing a session
        persist_mode = Xdp.PersistMode.PERSISTENT
        setup = self.create_session(
            params=params, restore_token=restore_token, persist_mode=persist_mode
        )

        session = setup.session
        start_done_invoked = False
        start_result, start_error = None, None

        def start_done(portal, task, data):
            nonlocal start_done_invoked
            nonlocal start_result, start_error

            start_done_invoked = True
            try:
                start_result = session.start_finish(task)
            except GLib.Error as e:
                start_error = e
            self.mainloop.quit()

        session.start(parent=None, cancellable=None, callback=start_done, data=None)

        self.mainloop.run()
        assert start_done_invoked
        assert start_error is None
        assert start_result is True

        # restore_tokenN is what the template uses
        assert session.get_restore_token() == "restore_token0"

        # FIXME: get_streams() returns a GVariant but how do you dereference that?
        session_streams = session.get_streams()
        for s_in, s_out in zip(session_streams, streams):
            assert s_in == s_out

        # We wanted persistent but we'll get transient from the template
        assert session.get_persist_mode() == Xdp.PersistMode.TRANSIENT

        method_calls = self.mock_interface.GetMethodCalls("Start")
        assert len(method_calls) == 1
        _, args = method_calls.pop(0)
        session_handle, parent_window, options = args
        assert parent_window == ""
        assert list(options.keys()) == ["handle_token"]

    def test_close_session(self):
        """
        Ensure that closing our session explicitly closes the session on DBus
        """
        setup = self.create_session()
        session = setup.session

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

        session.close()
        self.mainloop.run()
        assert was_closed is True
