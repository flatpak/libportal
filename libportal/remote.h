/*
 * Copyright (C) 2018, Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <portal-helpers.h>

G_BEGIN_DECLS

#define XDP_TYPE_SESSION (xdp_session_get_type ())

G_DECLARE_FINAL_TYPE (XdpSession, xdp_session, XDP, SESSION, GObject)

XDP_PUBLIC
GType xdp_session_get_type (void) G_GNUC_CONST;

/**
 * XdpOutputType:
 * @XDP_OUTPUT_MONITOR: allow selecting monitors
 * @XDP_OUTPUT_WINDOW: allow selecting individual application windows
 *
 * Flags to specify what kind of sources to offer for a screencast session.
 */
typedef enum {
  XDP_OUTPUT_MONITOR = 1 << 0,
  XDP_OUTPUT_WINDOW  = 1 << 1
} XdpOutputType;

/**
 * XdpDeviceType:
 * @XDP_DEVICE_NONE: no device
 * @XDP_DEVICE_KEYBOARD: control the keyboard.
 * @XDP_DEVICE_POINTER: control the pointer.
 * @XDP_DEVICE_TOUCHSCREEN: control the touchscreen.
 * 
 * Flags to specify what input devices to control for a remote desktop session.
 */
typedef enum {
  XDP_DEVICE_NONE        = 0,
  XDP_DEVICE_KEYBOARD    = 1 << 0,
  XDP_DEVICE_POINTER     = 1 << 1,
  XDP_DEVICE_TOUCHSCREEN = 1 << 2
} XdpDeviceType;

/**
 * XdpSessionType:
 * @XDP_SESSION_SCREENCAST: a screencast session.
 * @XDP_SESSION_REMOTE_DESKTOP: a remote desktop session.
 *
 * The type of a session.
 */
typedef enum {
  XDP_SESSION_SCREENCAST,
  XDP_SESSION_REMOTE_DESKTOP
} XdpSessionType;

/**
 * XdpSessionState:
 * @XDP_SESSION_INITIAL: the session has not been started.
 * @XDP_SESSION_ACTIVE: the session is active.
 * @XDP_SESSION_CLOSED: the session is no longer active.
 *
 * The state of a session.
 */
typedef enum {
  XDP_SESSION_INITIAL,
  XDP_SESSION_ACTIVE,
  XDP_SESSION_CLOSED
} XdpSessionState;

/**
 * XdpScreencastFlags:
 * @XDP_SCREENCAST_FLAG_NONE: No options
 * @XDP_SCREENCAST_FLAG_MULTIPLE: allow opening multiple streams
 *
 * Options for starting screen casts.
 */
typedef enum {
  XDP_SCREENCAST_FLAG_NONE     = 0,
  XDP_SCREENCAST_FLAG_MULTIPLE = 1 << 0
} XdpScreencastFlags;

XDP_PUBLIC
void        xdp_portal_create_screencast_session            (XdpPortal            *portal,
                                                             XdpOutputType         outputs,
                                                             XdpScreencastFlags    flags,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              data);

XDP_PUBLIC
XdpSession *xdp_portal_create_screencast_session_finish     (XdpPortal            *portal,
                                                             GAsyncResult         *result,
                                                             GError              **error);

/**
 * XdpRemoteDesktopFlags:
 * @XDP_REMOTE_DESKTOP_FLAG_NONE: No options
 * @XDP_REMOTE_DESKTOP_FLAG_MULTIPLE: allow opening multiple streams
 *
 * Options for starting remote desktop sessions.
 */
typedef enum {
  XDP_REMOTE_DESKTOP_FLAG_NONE     = 0,
  XDP_REMOTE_DESKTOP_FLAG_MULTIPLE = 1 << 0
} XdpRemoteDesktopFlags;

XDP_PUBLIC
void        xdp_portal_create_remote_desktop_session        (XdpPortal              *portal,
                                                             XdpDeviceType           devices,
                                                             XdpOutputType           outputs,
                                                             XdpRemoteDesktopFlags   flags,
                                                             GCancellable           *cancellable,
                                                             GAsyncReadyCallback     callback,
                                                             gpointer                data);

XDP_PUBLIC
XdpSession *xdp_portal_create_remote_desktop_session_finish (XdpPortal              *portal,
                                                             GAsyncResult           *result,
                                                             GError                **error);

XDP_PUBLIC
void        xdp_session_start                (XdpSession           *session,
                                              XdpParent            *parent,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              data);

XDP_PUBLIC
gboolean    xdp_session_start_finish         (XdpSession           *session,
                                              GAsyncResult         *result,
                                              GError              **error);

XDP_PUBLIC
void        xdp_session_close                (XdpSession           *session);

XDP_PUBLIC
int         xdp_session_open_pipewire_remote (XdpSession           *session);

XDP_PUBLIC
XdpSessionType  xdp_session_get_session_type  (XdpSession *session);

XDP_PUBLIC
XdpSessionState xdp_session_get_session_state (XdpSession *session);

XDP_PUBLIC
XdpDeviceType   xdp_session_get_devices       (XdpSession *session);

XDP_PUBLIC
GVariant *      xdp_session_get_streams       (XdpSession *session);

XDP_PUBLIC
void      xdp_session_pointer_motion    (XdpSession *session,
                                         double      dx,
                                         double      dy);

XDP_PUBLIC
void      xdp_session_pointer_position  (XdpSession *session,
                                         guint       stream,
                                         double      x,
                                         double      y);
/**
 * XdpButtonState:
 * @XDP_BUTTON_RELEASED: the button is down
 * @XDP_BUTTON_PRESSED: the button is up
 *
 * The XdpButtonState enumeration is used to describe
 * the state of buttons.
 */
typedef enum {
  XDP_BUTTON_RELEASED = 0,
  XDP_BUTTON_PRESSED = 1
} XdpButtonState;

XDP_PUBLIC
void      xdp_session_pointer_button (XdpSession     *session,
                                      int             button,
                                      XdpButtonState  state);

XDP_PUBLIC
void      xdp_session_pointer_axis   (XdpSession     *session,
                                      gboolean        finish,
                                      double          dx,
                                      double          dy);

/**
 * XdpDiscreteAxis:
 * @XDP_AXIS_HORIZONTAL_SCROLL: the horizontal scroll axis
 * @XDP_AXIS_VERTICAL_SCROLL: the horizontal scroll axis
 *
 * The XdpDiscreteAxis enumeration is used to describe
 * the discrete scroll axes.
 */
typedef enum {
  XDP_AXIS_HORIZONTAL_SCROLL = 0, 
  XDP_AXIS_VERTICAL_SCROLL   = 1 
} XdpDiscreteAxis;

XDP_PUBLIC
void      xdp_session_pointer_axis_discrete (XdpSession *session,
                                             XdpDiscreteAxis  axis,
                                             int              steps);

/**
 * XdpKeyState:
 * @XDP_KEY_RELEASED: the key is down
 * @XDP_KEY_PRESSED: the key is up
 *
 * The XdpKeyState enumeration is used to describe
 * the state of keys.
 */
typedef enum {
  XDP_KEY_RELEASED = 0,
  XDP_KEY_PRESSED = 1
} XdpKeyState;

XDP_PUBLIC
void      xdp_session_keyboard_key   (XdpSession *session,
                                      gboolean    keysym, 
                                      int         key,
                                      XdpKeyState state);

XDP_PUBLIC
void      xdp_session_touch_down     (XdpSession *session,
                                      guint       stream,
                                      guint       slot,
                                      double      x,
                                      double      y);

XDP_PUBLIC
void      xdp_session_touch_position (XdpSession *session,
                                      guint       stream,
                                      guint       slot,
                                      double      x,
                                      double      y);

XDP_PUBLIC
void      xdp_session_touch_up       (XdpSession *session,
                                      guint       slot);

G_END_DECLS
