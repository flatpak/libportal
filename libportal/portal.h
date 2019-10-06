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

/* Session */

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
void       xdp_portal_session_inhibit             (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   XdpInhibitFlags       inhibit,
                                                   const char           *reason,
                                                   const char           *id);

XDP_PUBLIC
void       xdp_portal_session_uninhibit           (XdpPortal            *portal,
                                                   const char           *id);


/**
 * XdpLoginSessionState:
 * @XDP_LOGIN_SESSION_RUNNING: the session is running
 * @XDP_LOGIN_SESSION_QUERY_END: the session is in the query end phase,
 *     during which applications can save their state or inhibit the
 *     session from ending
 * @XDP_LOGIN_SESSION_ENDING: the session is about to end
 *
 * The values of this enum are returned in the #XdpPortal::session-state-changed signal
 * to indicate the current state of the user session.
 */
typedef enum {
  XDP_LOGIN_SESSION_RUNNING =   1,
  XDP_LOGIN_SESSION_QUERY_END = 2,
  XDP_LOGIN_SESSION_ENDING =    3,
} XdpLoginSessionState;

XDP_PUBLIC
void       xdp_portal_session_monitor_start              (XdpPortal            *portal,
                                                          XdpParent            *parent,
                                                          GCancellable         *cancellable,
                                                          GAsyncReadyCallback   callback,
                                                          gpointer              data);

XDP_PUBLIC
gboolean   xdp_portal_session_monitor_start_finish       (XdpPortal            *portal,
                                                          GAsyncResult         *result,
                                                          GError              **error); 

XDP_PUBLIC
void       xdp_portal_session_monitor_stop               (XdpPortal            *portal);

XDP_PUBLIC
void       xdp_portal_session_monitor_query_end_response (XdpPortal            *portal);

/* OpenURI */

XDP_PUBLIC
void       xdp_portal_open_uri                    (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   const char           *uri,
                                                   gboolean              writable,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
gboolean  xdp_portal_open_uri_finish              (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);


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
                                                   const char           *path,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
gboolean  xdp_portal_trash_file_finish            (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);


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

/* Location */

#define XDP_TYPE_LOCATION (xdp_location_get_type ())

G_DECLARE_FINAL_TYPE (XdpLocation, xdp_location, XDP, LOCATION, GObject)

XDP_PUBLIC
GType xdp_location_get_type (void) G_GNUC_CONST;

/**
 * XdpLocationAccuracy:
 * @XDP_LOCATION_ACCURACY_NONE: No particular accuracy
 * @XDP_LOCATION_ACCURACY_COUNTRY: Country-level accuracy
 * @XDP_LOCATION_ACCURACY_CITY: City-level accuracy
 * @XDP_LOCATION_ACCURACY_NEIGHBORHOOD: Neighborhood-level accuracy
 * @XDP_LOCATION_ACCURACY_STREET: Street-level accuracy
 * @XDP_LOCATION_ACCURACY_EXACT: Maximum accuracy
 *
 * The values of this enum indicate the desired level
 * of accuracy for location information.
 */
typedef enum {
  XDP_LOCATION_ACCURACY_NONE,
  XDP_LOCATION_ACCURACY_COUNTRY,
  XDP_LOCATION_ACCURACY_CITY,
  XDP_LOCATION_ACCURACY_NEIGHBORHOOD,
  XDP_LOCATION_ACCURACY_STREET,
  XDP_LOCATION_ACCURACY_EXACT
} XdpLocationAccuracy;

XDP_PUBLIC
void         xdp_portal_create_location       (XdpPortal            *portal,
                                               guint                 distance_threshold,
                                               guint                 time_threshold,
                                               XdpLocationAccuracy   accuracy,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              data);

XDP_PUBLIC
XdpLocation *xdp_portal_create_location_finish (XdpPortal           *portal,
                                                GAsyncResult        *result,
                                                GError             **error);

XDP_PUBLIC
void         xdp_location_start               (XdpLocation          *location,
                                               XdpParent            *parent,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              data);

XDP_PUBLIC
gboolean     xdp_location_start_finish        (XdpLocation           *location,
                                               GAsyncResult         *result,
                                               GError              **error);

XDP_PUBLIC
void         xdp_location_close               (XdpLocation          *location);

/* Spawning */

/**
 * XdpSpawnFlags:
 * @XDP_SPAWN_FLAG_CLEARENV: Clear the environment
 * @XDP_SPAWN_FLAG_LATEST: Spawn the latest version of the app
 * @XDP_SPAWN_FLAG_SANDBOX: Spawn in a sandbox (equivalent to the --sandbox option of flatpak run)
 * @XDP_SPAWN_FLAG_NO_NETWORK: Spawn without network (equivalent to the --unshare=network option of flatpak run)
 * @XDP_SPAWN_FLAG_WATCH: Kill the sandbox when the caller disappears from the session bus
 *
 * Flags influencing the spawn operation and how the
 * new sandbox is created.
 */
typedef enum {
  XDP_SPAWN_FLAG_CLEARENV   = 1 << 0,
  XDP_SPAWN_FLAG_LATEST     = 1 << 1,
  XDP_SPAWN_FLAG_SANDBOX    = 1 << 2,
  XDP_SPAWN_FLAG_NO_NETWORK = 1 << 3,
  XDP_SPAWN_FLAG_WATCH      = 1 << 4,
} XdpSpawnFlags;

XDP_PUBLIC
void         xdp_portal_spawn                 (XdpPortal            *portal,
                                               const char           *cwd,
                                               const char * const   *argv,
                                               int                  *fds,
                                               int                  *map_to,
                                               int                   n_fds,
                                               const char * const   *env,
                                               XdpSpawnFlags         flags,
                                               const char * const   *sandbox_expose,
                                               const char * const   *sandbox_expose_ro,
                                               GCancellable         *cancellable,
                                               GAsyncReadyCallback   callback,
                                               gpointer              data);

XDP_PUBLIC
pid_t        xdp_portal_spawn_finish          (XdpPortal            *portal,
                                               GAsyncResult         *result,
                                               GError              **error);

XDP_PUBLIC
void        xdp_portal_spawn_signal           (XdpPortal            *portal,
                                               pid_t                 pid,
                                               int                   signal,
                                               gboolean              to_process_group);

/* Updates */

/**
 * XdpUpdateStatus:
 * @XDP_UPDATE_STATUS_RUNNING: Update in progress
 * @XDP_UPDATE_STATUS_EMPTY: No update to install
 * @XDP_UPDATE_STATUS_DONE: Update finished successfully
 * @XDP_UPDATE_STATUS_FAILED: Update failed
 *
 * The values of this enum are returned in the
 * #XdpPortal::update-progress signal to indicate
 * the current progress of an update installation.
 */
typedef enum {
  XDP_UPDATE_STATUS_RUNNING,
  XDP_UPDATE_STATUS_EMPTY,
  XDP_UPDATE_STATUS_DONE,
  XDP_UPDATE_STATUS_FAILED
} XdpUpdateStatus;

XDP_PUBLIC
void       xdp_portal_update_monitor_start        (XdpPortal            *portal,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
gboolean   xdp_portal_update_monitor_start_finish (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

XDP_PUBLIC
void       xdp_portal_update_monitor_stop         (XdpPortal            *portal);

XDP_PUBLIC
void       xdp_portal_update_install              (XdpPortal            *portal,
                                                   XdpParent            *parent,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              data);

XDP_PUBLIC
gboolean   xdp_portal_update_install_finish       (XdpPortal            *portal,
                                                   GAsyncResult         *result,
                                                   GError              **error);

/* OpenURI */

G_END_DECLS
