/*
 * Copyright (C) 2022, Matthew Leeds
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3.0 of the
 * License.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-only
 */

#include "config.h"

#include "dynamic-launcher.h"
#include "portal-private.h"

#define GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>


typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  char *name;
  char *url;
  char *token;
  GVariant *icon_v;
  XdpLauncherType launcher_type;
  gboolean editable_name;
  gboolean editable_icon;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;
} PrepareInstallLauncherCall;

static void
prepare_install_launcher_call_free (PrepareInstallLauncherCall *call)
{
  if (call->parent)
    {
      call->parent->parent_unexport (call->parent);
      xdp_parent_free (call->parent);
    }
  g_free (call->parent_handle);

  if (call->signal_id)
    g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);

  if (call->cancelled_id)
    g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);

  g_free (call->request_path);

  g_object_unref (call->portal);
  g_object_unref (call->task);

  g_variant_unref (call->icon_v);
  g_free (call->name);
  g_free (call->url);

  g_free (call);
}

static void
response_received (GDBusConnection *bus,
                   const char *sender_name,
                   const char *object_path,
                   const char *interface_name,
                   const char *signal_name,
                   GVariant *parameters,
                   gpointer data)
{
  PrepareInstallLauncherCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  if (call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    g_task_return_pointer (call->task, g_variant_ref (ret), (GDestroyNotify)g_variant_unref);
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Launcher install canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Launcher install failed");

  prepare_install_launcher_call_free (call);
}

static void do_prepare_install (PrepareInstallLauncherCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  PrepareInstallLauncherCall *call = data;
  call->parent_handle = g_strdup (handle);
  do_prepare_install (call);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  PrepareInstallLauncherCall *call = data;

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          call->request_path,
                          REQUEST_INTERFACE,
                          "Close",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL, NULL, NULL);

  g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                           "PrepareInstall call canceled by caller");

  prepare_install_launcher_call_free (call);
}

static void
prepare_install_returned (GObject      *object,
                          GAsyncResult *result,
                          gpointer      data)
{
  PrepareInstallLauncherCall *call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    {
      if (call->cancelled_id)
        {
          g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
          call->cancelled_id = 0;
        }
      g_task_return_error (call->task, error);
      prepare_install_launcher_call_free (call);
    }
}

static void
do_prepare_install (PrepareInstallLauncherCall *call)
{
  GVariantBuilder opt_builder;
  g_autofree char *handle_token = NULL;
  GCancellable *cancellable;

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, parent_exported, call);
      return;
    }

  handle_token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->request_path = g_strconcat (REQUEST_PATH_PREFIX, call->portal->sender, "/", handle_token, NULL);
  call->signal_id = g_dbus_connection_signal_subscribe (call->portal->bus,
                                                        PORTAL_BUS_NAME,
                                                        REQUEST_INTERFACE,
                                                        "Response",
                                                        call->request_path,
                                                        NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                        response_received,
                                                        call,
                                                        NULL);

  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (cancelled_cb), call);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (handle_token));
  g_variant_builder_add (&opt_builder, "{sv}", "launcher_type", g_variant_new_uint32 (call->launcher_type));
  if (call->url)
    g_variant_builder_add (&opt_builder, "{sv}", "target", g_variant_new_string (call->url));
  g_variant_builder_add (&opt_builder, "{sv}", "editable_name", g_variant_new_boolean (call->editable_name));
  g_variant_builder_add (&opt_builder, "{sv}", "editable_icon", g_variant_new_boolean (call->editable_icon));

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.DynamicLauncher",
                          "PrepareInstall",
                          g_variant_new ("(ssva{sv})",
                                         call->parent_handle,
                                         call->name,
                                         call->icon_v,
                                         &opt_builder),
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          prepare_install_returned,
                          call);
}

/**
 * xdp_portal_dynamic_launcher_prepare_install:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @name: the name for the launcher
 * @icon_v: a #GBytesIcon as returned by g_icon_serialize(). Must be a png or jpeg no larger than 512x512, or an svg
 * @launcher_type: the type of the launcher
 * @target: (nullable): the URL if the launcher is for a web app, or %NULL
 * @editable_name: if %TRUE, the user will be able to edit the name of the launcher
 * @editable_icon: if %TRUE, the user will be able to edit the icon of the launcher, if the implementation supports this
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Presents a dialog to the user so they can confirm they want to install a
 * launcher to their desktop.
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.dynamic_launcher_prepare_install_finish] to get the results.
 */
void
xdp_portal_dynamic_launcher_prepare_install (XdpPortal           *portal,
                                             XdpParent           *parent,
                                             const char          *name,
                                             GVariant            *icon_v,
                                             XdpLauncherType      launcher_type,
                                             const char          *target,
                                             gboolean             editable_name,
                                             gboolean             editable_icon,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             data)
{
  PrepareInstallLauncherCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (name != NULL && *name != '\0');
  g_return_if_fail (g_variant_is_of_type (icon_v, G_VARIANT_TYPE ("(sv)")));

  call = g_new0 (PrepareInstallLauncherCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->name = g_strdup (name);
  call->icon_v = g_variant_ref (icon_v);
  call->launcher_type = launcher_type;
  if (call->launcher_type == XDP_LAUNCHER_WEBAPP)
    call->url = g_strdup (target);
  call->editable_name = editable_name;
  call->editable_icon = editable_icon;
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_dynamic_launcher_prepare_install);

  do_prepare_install (call);
}

/**
 * xdp_portal_dynamic_launcher_prepare_install_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for a #GError
 *
 * Finishes the prepare-install-launcher request, and returns
 * [struct@GLib.Variant] dictionary with the following information:
 *
 * - name s: the name chosen by the user (or the provided name if the
 *     editable_name option was not set)
 * - token s: a token that can by used in a [method@Portal.dynamic_launcher_install]
 *     call to complete the installation
 *
 * Returns: (transfer full): a [struct@GLib.Variant] dictionary with launcher
 *   information
 */
GVariant *
xdp_portal_dynamic_launcher_prepare_install_finish (XdpPortal     *portal,
                                                    GAsyncResult  *result,
                                                    GError       **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_dynamic_launcher_prepare_install, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * xdp_portal_dynamic_launcher_request_install_token:
 * @portal: a [class@Portal]
 * @name: the name for the launcher
 * @icon_v: a #GBytesIcon as returned by g_icon_serialize(). Must be a png or jpeg no larger than 512x512, or an svg
 * @error: return location for a #GError
 *
 * Requests a token which can be passed to [method@Portal.dynamic_launcher_install]
 * to complete installation of the launcher without user interaction.
 *
 * This function only works when the caller's app ID is in the allowlist for
 * the portal backend being used. It's intended for software centers such as
 * GNOME Software or KDE Discover.
 *
 * Returns: (transfer full): a token that can be passed to
 *   [method@Portal.dynamic_launcher_install], or %NULL with @error set
 */
char *
xdp_portal_dynamic_launcher_request_install_token (XdpPortal   *portal,
                                                   const char  *name,
                                                   GVariant    *icon_v,
                                                   GError     **error)
{
  GVariantBuilder opt_builder;
  g_autoptr(GVariant) ret = NULL;
  g_autofree char *token = NULL;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (name != NULL && *name != '\0', NULL);
  g_return_val_if_fail (g_variant_is_of_type (icon_v, G_VARIANT_TYPE ("(sv)")), NULL);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  ret = g_dbus_connection_call_sync (portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     "org.freedesktop.portal.DynamicLauncher",
                                     "RequestInstallToken",
                                     g_variant_new ("(sva{sv})",
                                                    name,
                                                    icon_v,
                                                    &opt_builder),
                                     G_VARIANT_TYPE ("(s)"),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL, error);
  if (ret == NULL)
    return NULL;

  g_variant_get (ret, "(s)", &token);
  return g_steal_pointer (&token);
}

/**
 * xdp_portal_dynamic_launcher_install:
 * @portal: a [class@Portal]
 * @token: a token acquired via a [method@Portal.dynamic_launcher_request_install_token] or [method@Portal.dynamic_launcher_prepare_install] call
 * @desktop_file_id: the .desktop file name to be used
 * @desktop_entry: the key-file to be used for the contents of the .desktop file
 * @error: return location for a #GError
 *
 * This function completes installation of a launcher so that the icon and name
 * given in previous method calls will show up in the desktop environment's menu.
 *
 * The @desktop_file_id must be prefixed with the caller's app ID followed by a
 * "." and suffixed with ".desktop".
 *
 * The @desktop_entry data need not include Icon= or Name= entries since these
 * will be added by the portal, and the Exec= entry will be rewritten to call
 * the application with e.g. "flatpak run" depending on the sandbox status of
 * the app.
 *
 * Returns: %TRUE if the installation was successful, %FALSE with @error set
 *   otherwise
 */
gboolean
xdp_portal_dynamic_launcher_install (XdpPortal   *portal,
                                     const char  *token,
                                     const char  *desktop_file_id,
                                     const char  *desktop_entry,
                                     GError     **error)
{
  GVariantBuilder opt_builder;
  g_autoptr(GVariant) ret = NULL;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (token != NULL && *token != '\0', FALSE);
  g_return_val_if_fail (desktop_file_id != NULL && *desktop_file_id != '\0', FALSE);
  g_return_val_if_fail (desktop_entry != NULL && *desktop_entry != '\0', FALSE);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  ret = g_dbus_connection_call_sync (portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     "org.freedesktop.portal.DynamicLauncher",
                                     "Install",
                                     g_variant_new ("(sssa{sv})",
                                                    token,
                                                    desktop_file_id,
                                                    desktop_entry,
                                                    &opt_builder),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL, error);
  return (ret != NULL);
}

/**
 * xdp_portal_dynamic_launcher_uninstall:
 * @portal: a [class@Portal]
 * @desktop_file_id: the .desktop file name
 * @error: return location for a #GError
 *
 * This function uninstalls a launcher that was previously installed using the
 * dynamic launcher portal, resulting in the .desktop file and icon being deleted.
 *
 * The @desktop_file_id must be prefixed with the caller's app ID followed by a
 * "." and suffixed with ".desktop".
 *
 * Returns: %TRUE if the uninstallation was successful, %FALSE with @error set
 *   otherwise
 */
gboolean
xdp_portal_dynamic_launcher_uninstall (XdpPortal   *portal,
                                       const char  *desktop_file_id,
                                       GError     **error)
{
  GVariantBuilder opt_builder;
  g_autoptr(GVariant) ret = NULL;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (desktop_file_id != NULL && *desktop_file_id != '\0', FALSE);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  ret = g_dbus_connection_call_sync (portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     "org.freedesktop.portal.DynamicLauncher",
                                     "Uninstall",
                                     g_variant_new ("(sa{sv})",
                                                    desktop_file_id,
                                                    &opt_builder),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL, error);
  return (ret != NULL);
}

/**
 * xdp_portal_dynamic_launcher_get_desktop_entry:
 * @portal: a [class@Portal]
 * @desktop_file_id: the .desktop file name
 * @error: return location for a #GError
 *
 * This function gets the contents of a .desktop file that was previously
 * installed by the dynamic launcher portal.
 *
 * The @desktop_file_id must be prefixed with the caller's app ID followed by a
 * "." and suffixed with ".desktop".
 *
 * Returns: (transfer full): the contents of the desktop file, or %NULL with
 *   @error set
 */
char *
xdp_portal_dynamic_launcher_get_desktop_entry (XdpPortal   *portal,
                                               const char  *desktop_file_id,
                                               GError     **error)
{
  g_autoptr(GVariant) ret = NULL;
  g_autofree char *contents = NULL;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (desktop_file_id != NULL && *desktop_file_id != '\0', NULL);

  ret = g_dbus_connection_call_sync (portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     "org.freedesktop.portal.DynamicLauncher",
                                     "GetDesktopEntry",
                                     g_variant_new ("(s)", desktop_file_id),
                                     G_VARIANT_TYPE ("(s)"),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL, error);
  if (ret == NULL)
    return NULL;

  g_variant_get (ret, "(s)", &contents);
  return g_steal_pointer (&contents);
}

/**
 * xdp_portal_dynamic_launcher_get_icon:
 * @portal: a [class@Portal]
 * @desktop_file_id: the .desktop file name
 * @out_icon_format: (nullable): return location for icon format string, one of "png", "jpeg", "svg"
 * @out_icon_size: (nullable): return location for icon size
 * @error: return location for a #GError
 *
 * This function gets the icon associated with a .desktop file that was previously
 * installed by the dynamic launcher portal.
 *
 * The @desktop_file_id must be prefixed with the caller's app ID followed by a
 * "." and suffixed with ".desktop".
 *
 * Returns: (transfer full): the icon in a format recognized by g_icon_deserialize(),
 *   or %NULL with @error set
 */
GVariant *
xdp_portal_dynamic_launcher_get_icon (XdpPortal   *portal,
                                      const char  *desktop_file_id,
                                      char       **out_icon_format,
                                      guint       *out_icon_size,
                                      GError     **error)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariant) icon_v = NULL;
  g_autofree char *icon_format = NULL;
  guint icon_size;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (desktop_file_id != NULL && *desktop_file_id != '\0', NULL);

  ret = g_dbus_connection_call_sync (portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     "org.freedesktop.portal.DynamicLauncher",
                                     "GetIcon",
                                     g_variant_new ("(s)", desktop_file_id),
                                     G_VARIANT_TYPE ("(vsu)"),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL, error);
  if (ret == NULL)
    return NULL;

  g_variant_get (ret, "(vsu)", &icon_v, &icon_format, &icon_size);

  if (out_icon_format)
    *out_icon_format = g_steal_pointer (&icon_format);
  if (out_icon_size)
    *out_icon_size = icon_size;

  return g_steal_pointer (&icon_v);
}

/**
 * xdp_portal_launch:
 * @portal: a [class@Portal]
 * @desktop_file_id: the .desktop file name
 * @activation_token: (nullable): the activation token, see the "XDG activation" section of the wayland-protocols docs
 * @error: return location for a #GError
 *
 * This function launches the app referred to by @desktop_file_id.
 *
 * The @desktop_file_id must be prefixed with the caller's app ID followed by a
 * "." and suffixed with ".desktop".
 *
 * Returns: %TRUE if the uninstallation was successful, %FALSE with @error set
 *   otherwise
 */
gboolean
xdp_portal_dynamic_launcher_launch (XdpPortal   *portal,
                                    const char  *desktop_file_id,
                                    const char  *activation_token,
                                    GError     **error)
{
  g_autoptr(GVariant) ret = NULL;
  GVariantBuilder opt_builder;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (desktop_file_id != NULL && *desktop_file_id != '\0', FALSE);

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
  if (activation_token != NULL && *activation_token != '\0')
    g_variant_builder_add (&opt_builder, "{sv}", "activation_token", g_variant_new_string (activation_token));

  ret = g_dbus_connection_call_sync (portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     "org.freedesktop.portal.DynamicLauncher",
                                     "Launch",
                                     g_variant_new ("(sa{sv})",
                                                    desktop_file_id,
                                                    &opt_builder),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1,
                                     NULL, error);
  return (ret != NULL);
}
