/*
 * Copyright (C) 2019, Matthias Clasen
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

#include "wallpaper.h"

#define GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>

#include "portal-private.h"

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  char *uri;
  gboolean show_preview;
  XdpWallpaperFlags target;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;
} WallpaperCall;

static void
wallpaper_call_free (WallpaperCall *call)
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
  g_free (call->uri);

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
  WallpaperCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  if (call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    g_task_return_boolean (call->task, TRUE);
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "SetWallpaper canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "SetWallpaper failed");

  wallpaper_call_free (call);
}

static void set_wallpaper (WallpaperCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  WallpaperCall *call = data;
  call->parent_handle = g_strdup (handle);
  set_wallpaper (call);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  WallpaperCall *call = data;

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

  g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "SetWallpaper call canceled by caller");

  wallpaper_call_free (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  WallpaperCall *call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GFile) file = NULL;

  file = g_file_new_for_uri (call->uri);
  if (g_file_is_native (file))
    ret = g_dbus_connection_call_with_unix_fd_list_finish (G_DBUS_CONNECTION (object), NULL, result, &error);
  else
    ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);

  if (error)
    {
      if (call->cancelled_id)
        {
          g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
          call->cancelled_id = 0;
        }

      g_task_return_error (call->task, error);
      wallpaper_call_free (call);
    }
}

#ifndef O_PATH
#define O_PATH 0
#endif

static const char *
target_to_string (XdpWallpaperFlags target)
{
  if (((target & XDP_WALLPAPER_FLAG_BACKGROUND) != 0) &&
      ((target & XDP_WALLPAPER_FLAG_LOCKSCREEN) != 0))
    return "both";
  else if ((target & XDP_WALLPAPER_FLAG_BACKGROUND) != 0)
    return "background";
  else if ((target & XDP_WALLPAPER_FLAG_LOCKSCREEN) != 0)
    return "lockscreen";
  else
    {
      g_warning ("Unknown XdpWallpaperTarget value");
      return "both";
    }
}

static void
set_wallpaper (WallpaperCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autoptr(GFile) file = NULL;
  GCancellable *cancellable;

  if (call->parent_handle == NULL)
    {
      call->parent->parent_export (call->parent, parent_exported, call);
      return;
    }

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->request_path = g_strconcat (REQUEST_PATH_PREFIX, call->portal->sender, "/", token, NULL);
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

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  g_variant_builder_add (&options, "{sv}", "show-preview", g_variant_new_boolean (call->show_preview));
  g_variant_builder_add (&options, "{sv}", "set-on", g_variant_new_string (target_to_string (call->target)));

  file = g_file_new_for_uri (call->uri);

  if (g_file_is_native (file))
    {
      g_autoptr(GUnixFDList) fd_list = NULL;
      g_autofree char *path = NULL;
      int fd, fd_in;

      path = g_file_get_path (file);

      fd = g_open (path, O_PATH | O_CLOEXEC);
      if (fd == -1)
        {
          g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to open '%s'", call->uri);
          wallpaper_call_free (call);

          return;
        }

      fd_list = g_unix_fd_list_new_from_array (&fd, 1);
      fd = -1;
      fd_in = 0;

      g_dbus_connection_call_with_unix_fd_list (call->portal->bus,
                                                PORTAL_BUS_NAME,
                                                PORTAL_OBJECT_PATH,
                                                "org.freedesktop.portal.Wallpaper",
                                                "SetWallpaperFile",
                                                g_variant_new ("(sha{sv})", call->parent_handle, fd_in, &options),
                                                G_VARIANT_TYPE ("(o)"),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                fd_list,
                                                NULL,
                                                call_returned,
                                                call);
    }
  else
    {
      g_dbus_connection_call (call->portal->bus,
                              PORTAL_BUS_NAME,
                              PORTAL_OBJECT_PATH,
                              "org.freedesktop.portal.Wallpaper",
                              "SetWallpaperURI",
                              g_variant_new ("(ssa{sv})", call->parent_handle, call->uri, &options),
                              G_VARIANT_TYPE ("(o)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL,
                              call_returned,
                              call);
    }
}

/**
 * xdp_portal_set_wallpaper:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @uri: the URI to use
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Sets a desktop background image, given by a uri.
 */
void
xdp_portal_set_wallpaper (XdpPortal           *portal,
                          XdpParent           *parent,
                          const char          *uri,
                          XdpWallpaperFlags    flags,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             data)
{
  WallpaperCall *call = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail ((flags & ~(XDP_WALLPAPER_FLAG_BACKGROUND |
                               XDP_WALLPAPER_FLAG_LOCKSCREEN |
                               XDP_WALLPAPER_FLAG_PREVIEW)) == 0); 

  call = g_new0 (WallpaperCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->uri = g_strdup (uri);
  call->show_preview = (flags & XDP_WALLPAPER_FLAG_PREVIEW) != 0;
  call->target = flags & (XDP_WALLPAPER_FLAG_BACKGROUND | XDP_WALLPAPER_FLAG_LOCKSCREEN);
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_set_wallpaper);

  set_wallpaper (call);
}

/**
 * xdp_portal_set_wallpaper_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the open-uri request.
 *
 * Returns the result in the form of a boolean.
 *
 * Returns: `TRUE` if the call succeeded
 */
gboolean
xdp_portal_set_wallpaper_finish (XdpPortal *portal,
                                 GAsyncResult *result,
                                 GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_set_wallpaper, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
