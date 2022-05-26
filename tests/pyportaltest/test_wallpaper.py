# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from . import PortalTest

import gi
import logging

gi.require_version("Xdp", "1.0")
from gi.repository import GLib, Xdp

logger = logging.getLogger(__name__)


class TestWallpaper(PortalTest):
    def test_version(self):
        self.assert_version_eq(1)

    def set_wallpaper(
        self, uri_to_set: str, set_on: Xdp.WallpaperFlags, show_preview: bool
    ):
        params = {}
        self.setup_daemon(params)

        xdp = Xdp.Portal.new()
        assert xdp is not None

        flags = {
            "background": Xdp.WallpaperFlags.BACKGROUND,
            "lockscreen": Xdp.WallpaperFlags.LOCKSCREEN,
            "both": Xdp.WallpaperFlags.BACKGROUND | Xdp.WallpaperFlags.LOCKSCREEN,
        }[set_on]

        if show_preview:
            flags |= Xdp.WallpaperFlags.PREVIEW

        wallpaper_was_set = False

        def set_wallpaper_done(portal, task, data):
            nonlocal wallpaper_was_set
            wallpaper_was_set = portal.set_wallpaper_finish(task)
            self.mainloop.quit()

        xdp.set_wallpaper(
            parent=None,
            uri=uri_to_set,
            flags=flags,
            cancellable=None,
            callback=set_wallpaper_done,
            data=None,
        )

        self.mainloop.run()

        method_calls = self.mock_interface.GetMethodCalls("SetWallpaperURI")
        assert len(method_calls) == 1
        timestamp, args = method_calls.pop(0)
        parent, uri, options = args
        assert uri == uri_to_set
        assert options["set-on"] == set_on
        assert options["show-preview"] == show_preview

        assert wallpaper_was_set

    def test_set_wallpaper_background(self):
        self.set_wallpaper("https://background.nopreview", "background", False)

    def test_set_wallpaper_background_preview(self):
        self.set_wallpaper("https://background.preview", "background", True)

    def test_set_wallpaper_lockscreen(self):
        self.set_wallpaper("https://lockscreen.nopreview", "lockscreen", False)

    def test_set_wallpaper_lockscreen_preview(self):
        self.set_wallpaper("https://lockscreen.preview", "lockscreen", True)

    def test_set_wallpaper_both(self):
        self.set_wallpaper("https://both.nopreview", "both", False)

    def test_set_wallpaper_both_preview(self):
        self.set_wallpaper("https://both.preview", "both", True)

    def test_set_wallpaper_cancel(self):
        params = {"response": 1}
        self.setup_daemon(params)

        xdp = Xdp.Portal.new()
        assert xdp is not None

        flags = Xdp.WallpaperFlags.BACKGROUND

        wallpaper_was_set = False

        def set_wallpaper_done(portal, task, data):
            nonlocal wallpaper_was_set
            try:
                wallpaper_was_set = portal.set_wallpaper_finish(task)
            except GLib.GError:
                pass
            self.mainloop.quit()

        xdp.set_wallpaper(
            parent=None,
            uri="https://ignored.anyway",
            flags=flags,
            cancellable=None,
            callback=set_wallpaper_done,
            data=None,
        )

        self.mainloop.run()

        method_calls = self.mock_interface.GetMethodCalls("SetWallpaperURI")
        assert len(method_calls) == 1

        assert not wallpaper_was_set
