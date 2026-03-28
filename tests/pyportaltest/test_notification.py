# SPDX-License-Identifier: LGPL-3.0-only
#
# This file is formatted with Python Black

from . import PortalTest

import gi
import logging

gi.require_version("Xdp", "1.0")
from gi.repository import GLib, Gio, GioUnix, Xdp

logger = logging.getLogger(__name__)

SVG_IMAGE_DATA = ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<svg xmlns=\"http://www.w3.org/2000/svg\" height=\"16px\" width=\"16px\"/>")


class TestNotification(PortalTest):
    def test_version(self):
        self.assert_version_eq(2)

    def add_notification(
            self, version: int, id_to_set: str, notification_to_set: GLib.Variant
    ):
        params = {"version": version}
        self.setup_daemon(params)

        xdp = Xdp.Portal.new()
        assert xdp is not None

        notification_added = False

        def add_notification_done(portal, task, data):
            nonlocal notification_added
            notification_added = portal.add_notification_finish(task)
            self.mainloop.quit()

        xdp.add_notification(
            id=id_to_set,
            notification=notification_to_set,
            flags=Xdp.NotificationFlags.NONE,
            cancellable=None,
            callback=add_notification_done,
            data=None,
        )

        self.mainloop.run()

        method_calls = self.mock_interface.GetMethodCalls("AddNotification")
        assert len(method_calls) == 1
        timestamp, args = method_calls.pop(0)
        id, notification = args
        assert id == id_to_set
        assert notification_added

        return notification

    def test_add_notification(self):
        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(2, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        notification_to_set_dict.pop('invalid-property', None)
        assert dict(notification) == notification_to_set_dict

    def test_add_notification_bytes_icon(self):
        bytes = GLib.Bytes.new(SVG_IMAGE_DATA.encode('utf-8'))
        icon = Gio.BytesIcon.new(bytes);

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'icon': icon.serialize(),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(2, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        icon_to_set = notification_to_set_dict.pop('icon', None)
        icon = notification.pop('icon', None)
        assert dict(notification) == notification_to_set_dict
        (icon_type, handle) = icon
        assert icon_type == 'file-descriptor'
        input_stream = GioUnix.InputStream.new(handle.take(), True)

        res_bytes = input_stream.read_bytes(bytes.get_size(), None)
        assert res_bytes.equal (bytes)

    def test_add_notification_file_icon(self):
        bytes = GLib.Bytes.new(SVG_IMAGE_DATA.encode('utf-8'))

        (file, iostream) = Gio.File.new_tmp("iconXXXXXX")
        stream = iostream.get_output_stream()
        stream.write_bytes(bytes)
        stream.close()
        icon = Gio.FileIcon.new (file)

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'icon': icon.serialize(),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                          )
        notification = self.add_notification(2, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        icon_to_set = notification_to_set_dict.pop('icon', None)
        icon = notification.pop('icon', None)
        assert dict(notification) == notification_to_set_dict
        (icon_type, handle) = icon
        assert icon_type == 'file-descriptor'
        input_stream = GioUnix.InputStream.new(handle.take(), True)

        res_bytes = input_stream.read_bytes(bytes.get_size(), None)
        assert res_bytes.equal (bytes)

        file.delete()

    def test_add_notification_themed_icon(self):
        icon = Gio.ThemedIcon.new('someicon')

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'icon': icon.serialize(),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(2, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        icon_to_set = notification_to_set_dict.pop('icon', None)
        icon_out = notification.pop('icon', None)
        assert dict(notification) == notification_to_set_dict
        (icon_type, names) = icon_out
        assert icon_type == 'themed'

        assert names == icon.get_names()

    def test_add_notification_bytes_sound(self):
        bytes = GLib.Bytes.new(SVG_IMAGE_DATA.encode('utf-8'))
        icon = Gio.BytesIcon.new(bytes);

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'sound': GLib.Variant('(sv)', ('bytes', GLib.Variant('ay', bytes.get_data()))),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(2, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        sound_to_set = notification_to_set_dict.pop('sound', None)
        sound = notification.pop('sound', None)
        assert dict(notification) == notification_to_set_dict
        (sound_type, handle) = sound
        assert sound_type == 'file-descriptor'
        input_stream = GioUnix.InputStream.new(handle.take(), True)

        res_bytes = input_stream.read_bytes(bytes.get_size(), None)
        assert res_bytes.equal (bytes)

    def test_add_notification_file_sound(self):
        bytes = GLib.Bytes.new(SVG_IMAGE_DATA.encode('utf-8'))

        (file, iostream) = Gio.File.new_tmp("soundXXXXXX")
        stream = iostream.get_output_stream()
        stream.write_bytes(bytes)
        stream.close()

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'sound': GLib.Variant('(sv)', ['file', GLib.Variant('s', file.get_uri())]),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                         )
        notification = self.add_notification(2, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        sound_to_set = notification_to_set_dict.pop('sound', None)
        sound_out = notification.pop('sound', None)
        assert dict(notification) == notification_to_set_dict
        (sound_type, handle) = sound_out
        assert sound_type == 'file-descriptor'
        input_stream = GioUnix.InputStream.new(handle.take(), True)

        res_bytes = input_stream.read_bytes(bytes.get_size(), None)
        assert res_bytes.equal (bytes)

        file.delete()

    def test_add_notification_v1(self):
        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'invalid-property': GLib.Variant('s', 'invalid'),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )

        notification = self.add_notification(1, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        notification_to_set_dict.pop('invalid-property', None)
        assert dict(notification) == notification_to_set_dict

    def test_add_notification_bytes_icon_v1(self):
        bytes = GLib.Bytes.new(SVG_IMAGE_DATA.encode('utf-8'))
        icon = Gio.BytesIcon.new(bytes);

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'icon': icon.serialize(),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(1, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        icon_to_set = notification_to_set_dict.pop('icon', None)
        icon = notification.pop('icon', None)
        assert dict(notification) == notification_to_set_dict
        (icon_type, res_bytes) = icon
        assert icon_type == 'bytes'
        assert GLib.Bytes(res_bytes).equal (bytes)

    def test_add_notification_file_icon_v1(self):
        bytes = GLib.Bytes.new(SVG_IMAGE_DATA.encode('utf-8'))

        (file, iostream) = Gio.File.new_tmp("iconXXXXXX")
        stream = iostream.get_output_stream()
        stream.write_bytes(bytes)
        stream.close()
        icon = Gio.FileIcon.new (file)

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'icon': icon.serialize(),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(1, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        icon_to_set = notification_to_set_dict.pop('icon', None)
        icon = notification.pop('icon', None)
        assert dict(notification) == notification_to_set_dict
        (icon_type, res_bytes) = icon
        assert icon_type == 'bytes'
        assert GLib.Bytes(res_bytes).equal (bytes)

        file.delete()

    def test_add_notification_themed_icon_v1(self):
        icon = Gio.ThemedIcon.new('someicon')

        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'icon': icon.serialize(),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(2, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        icon_to_set = notification_to_set_dict.pop('icon', None)
        icon_out = notification.pop('icon', None)
        assert dict(notification) == notification_to_set_dict
        (icon_type, names) = icon_out
        assert icon_type == 'themed'

        assert names == icon.get_names()

    def test_add_notification_buttons(self):
        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'buttons': GLib.Variant('aa{sv}', [
                                                {
                                                    'label': GLib.Variant('s', 'button1'),
                                                    'action': GLib.Variant('s', 'action1'),
                                                    'purpose': GLib.Variant('s', 'some-purpose')
                                                    },
                                                {
                                                    'label': GLib.Variant('s', 'button2'),
                                                    'action': GLib.Variant('s', 'action2'),
                                                    'purpose': GLib.Variant('s', 'some-purpose')
                                                    },
                                                ]),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )
        notification = self.add_notification(2, 'someid', notification_to_set)

        assert dict(notification) == dict(notification_to_set)

    def test_add_notification_buttons_v1(self):
        notification_to_set = GLib.Variant('a{sv}',
                                           {'title': GLib.Variant('s', 'title'),
                                            'body': GLib.Variant('s', 'test notification body'),
                                            'priority': GLib.Variant('s', 'normal'),
                                            'buttons': GLib.Variant('aa{sv}', [
                                                {
                                                    'label': GLib.Variant('s', 'button1'),
                                                    'action': GLib.Variant('s', 'action1'),
                                                    'purpose': GLib.Variant('s', 'some-purpose')
                                                    },
                                                {
                                                    'label': GLib.Variant('s', 'button2'),
                                                    'action': GLib.Variant('s', 'action2'),
                                                    'purpose': GLib.Variant('s', 'some-purpose')
                                                    },
                                                ]),
                                            'default-action': GLib.Variant('s', 'test-action')}
                                           )

        notification = self.add_notification(1, 'someid', notification_to_set)

        notification_to_set_dict = dict(notification_to_set)
        buttons = notification_to_set_dict.pop('buttons', None)

        notification_to_set_dict['buttons'] = []
        for button in buttons:
            button.pop('purpose', None)
            notification_to_set_dict['buttons'].append(button)

        assert dict(notification) == notification_to_set_dict
