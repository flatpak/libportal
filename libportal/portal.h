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

#include <gio/gio.h>

G_BEGIN_DECLS

#define XDP_TYPE_PORTAL (xdp_portal_get_type ())

G_DECLARE_FINAL_TYPE (XdpPortal, xdp_portal, XDP, PORTAL, GObject)

XDP_PUBLIC
GType      xdp_portal_get_type               (void) G_GNUC_CONST;

XDP_PUBLIC
XdpPortal *xdp_portal_new                    (void);

/**
 * XdpParent:
 *
 * A struct that provides information about parent windows.
 * The members of this struct are private to libportal and should
 * not be accessed by applications.
 */
typedef struct _XdpParent XdpParent;

typedef void     (* XdpParentExported) (XdpParent         *parent,
                                        const char        *handle,
                                        gpointer           data);
typedef gboolean (* XdpParentExport)   (XdpParent         *parent,
                                        XdpParentExported  callback,
                                        gpointer           data);
typedef void     (* XdpParentUnexport) (XdpParent         *parent);

struct _XdpParent {
  /*< private >*/
  XdpParentExport export;
  XdpParentUnexport unexport;
  GObject *object;
  XdpParentExported callback;
  gpointer data;
};

static inline void xdp_parent_free (XdpParent *parent);

static inline void xdp_parent_free (XdpParent *parent)
{
  g_clear_object (&parent->data);
  g_free (parent);
}

/* Screenshot */

XDP_PUBLIC
void       xdp_portal_take_screenshot        (XdpPortal           *portal,
                                              XdpParent           *parent,
                                              gboolean             modal,
                                              gboolean             interactive,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             data);

XDP_PUBLIC
char *     xdp_portal_take_screenshot_finish (XdpPortal           *portal,
                                              GAsyncResult        *result,
                                              GError             **error);

XDP_PUBLIC
void       xdp_portal_pick_color             (XdpPortal           *portal,
                                              XdpParent           *parent,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             data);

XDP_PUBLIC
GVariant * xdp_portal_pick_color_finish      (XdpPortal           *portal,
                                              GAsyncResult        *result,
                                              GError             **error);


/* Background */

XDP_PUBLIC
void      xdp_portal_request_background         (XdpPortal           *portal,
                                                 XdpParent           *parent,
                                                 GPtrArray           *commandline,
                                                 char                *reason,
                                                 gboolean             autostart,
                                                 gboolean             dbus_activatable,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data);

XDP_PUBLIC
gboolean   xdp_portal_request_background_finish (XdpPortal     *portal,
                                                 GAsyncResult  *result,
                                                 GError       **error);

/* Notification */

XDP_PUBLIC
void       xdp_portal_add_notification    (XdpPortal  *portal,
                                           const char *id,
                                           GVariant   *notification);

XDP_PUBLIC
void       xdp_portal_remove_notification (XdpPortal  *portal,
                                           const char *id);

/* Email */

XDP_PUBLIC
void       xdp_portal_compose_email        (XdpPortal            *portal,
                                            XdpParent            *parent,
                                            const char           *address,
                                            const char           *subject,
                                            const char           *body,
                                            const char *const    *attachments,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              data);

XDP_PUBLIC
gboolean   xdp_portal_compose_email_finish (XdpPortal            *portal,
                                            GAsyncResult         *result,
                                            GError              **error);

/* Account */

XDP_PUBLIC
void       xdp_portal_get_user_information        (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *reason,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
GVariant * xdp_portal_get_user_information_finish (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

/* Inhibit */

/**
 * XdpInhibitFlags:
 * @XDP_INHIBIT_LOGOUT: Inhibit logout.
 * @XDP_INHIBIT_USER_SWITCH: Inhibit user switching.
 * @XDP_INHIBIT_SUSPEND: Inhibit suspend.
 * @XDP_INHIBIT_IDLE: Inhibit the session going idle.
 *
 * Flags that determine what session status changes are inhibited.
 */
typedef enum {
  XDP_INHIBIT_LOGOUT      = 1,
  XDP_INHIBIT_USER_SWITCH = 2,
  XDP_INHIBIT_SUSPEND     = 4,
  XDP_INHIBIT_IDLE        = 8
} XdpInhibitFlags;

XDP_PUBLIC
void       xdp_portal_inhibit                     (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   XdpInhibitFlags       inhibit,
                                                   const char           *reason,
                                                   const char           *id);

XDP_PUBLIC
void       xdp_portal_uninhibit                   (XdpPortal            *portal,
                                                   const char           *id);

/* OpenURI */

XDP_PUBLIC
void       xdp_portal_open_uri                    (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *uri,
                                                   gboolean              writable);

/* Filechooser */

XDP_PUBLIC
void       xdp_portal_open_file                   (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *title,
                                                   gboolean              modal,
                                                   gboolean              multiple,
                                                   GVariant             *filters,
                                                   GVariant             *choices,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
GVariant *xdp_portal_open_file_finish             (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

XDP_PUBLIC
void       xdp_portal_save_file                   (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *title,
                                                   gboolean              modal,
                                                   const char           *current_name,
                                                   const char           *current_folder,
                                                   const char           *current_file,
                                                   GVariant             *filters,
                                                   GVariant             *choices,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
GVariant *xdp_portal_save_file_finish             (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

/* Trash */

XDP_PUBLIC
void     xdp_portal_trash_file                    (XdpPortal            *portal,
                                                   const char           *path);

/* Print */

XDP_PUBLIC
void      xdp_portal_prepare_print                (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *title,
                                                   gboolean              modal,
                                                   GVariant             *settings,
                                                   GVariant             *page_setup,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
GVariant *xdp_portal_prepare_print_finish         (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

XDP_PUBLIC
void      xdp_portal_print_file                   (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *title,
                                                   gboolean              modal,
                                                   guint                 token,
                                                   const char           *file,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
gboolean xdp_portal_print_file_finish             (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

/* Remote desktop, screencast */

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
  XDP_OUTPUT_MONITOR = 1,
  XDP_OUTPUT_WINDOW  = 2
} XdpOutputType;

/**
 * XDP_OUTPUT_NONE:
 *
 * The value to use as null value for XdpOutputType.
 */
#define XDP_OUTPUT_NONE 0

/**
 * XDP_OUTPUT_ALL:
 *
 * A convenient value to select all possible kinds of output.
 */
#define XDP_OUTPUT_ALL  (XDP_OUTPUT_MONITOR | XDP_OUTPUT_WINDOW)

/**
 * XdpDeviceType:
 * @XDP_DEVICE_KEYBOARD: control the keyboard.
 * @XDP_DEVICE_POINTER: control the pointer.
 * @XDP_DEVICE_TOUCHSCREEN: control the touchscreen.
 * 
 * Flags to specify what input devices to control for a remote desktop session.
 */
typedef enum {
  XDP_DEVICE_KEYBOARD    = 1,
  XDP_DEVICE_POINTER     = 2,
  XDP_DEVICE_TOUCHSCREEN = 4
} XdpDeviceType;

/**
 * XDP_DEVICE_NONE:
 *
 * The value to use as null value for XdpDeviceType.
 */
#define XDP_DEVICE_NONE 0

/**
 * XDP_DEVICE_ALL:
 *
 * A convenient value to select all possible input devices.
 */
#define XDP_DEVICE_ALL (XDP_DEVICE_KEYBOARD | XDP_DEVICE_POINTER | XDP_DEVICE_TOUCHSCREEN)

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

XDP_PUBLIC
void        xdp_portal_create_screencast_session            (XdpPortal            *portal,
                                                             XdpOutputType         outputs,
                                                             gboolean              multiple,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              data);

XDP_PUBLIC
XdpSession *xdp_portal_create_screencast_session_finish     (XdpPortal            *portal,
                                                             GAsyncResult         *result,
                                                             GError              **error);

XDP_PUBLIC
void        xdp_portal_create_remote_desktop_session        (XdpPortal            *portal,
                                                             XdpDeviceType         devices,
                                                             XdpOutputType         outputs,
                                                             gboolean              multiple,
                                                             GCancellable         *cancellable,
                                                             GAsyncReadyCallback   callback,
                                                             gpointer              data);

XDP_PUBLIC
XdpSession *xdp_portal_create_remote_desktop_session_finish (XdpPortal            *portal,
                                                             GAsyncResult         *result,
                                                             GError              **error);

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

/* Camera */

XDP_PUBLIC
gboolean  xdp_portal_is_camera_present   (XdpPortal            *portal);

XDP_PUBLIC
void      xdp_portal_access_camera       (XdpPortal            *portal,
                                          XdpParent            *parent,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              data);

XDP_PUBLIC
gboolean xdp_portal_access_camera_finish (XdpPortal            *portal,
                                          GAsyncResult         *result,
                                          GError              **error);

XDP_PUBLIC
int      xdp_portal_open_pipewire_remote_for_camera (XdpPortal *portal);

G_END_DECLS
