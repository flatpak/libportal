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

#include "config.h"

#define GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>

#include "portal-private.h"
#include "utils-private.h"

/**
 * SECTION:open
 * @title: Open
 * @short_description: handle URIs
 *
 * These functions let applications open URIs in external handlers.
 * A typical example is to open a pdf file in a document viewer.
 *
 * The underlying portal is org.freedesktop.portal.OpenURI.
 */

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  char *uri;
  gboolean writable;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;
} OpenCall;

static void
open_call_free (OpenCall *call)
{
  if (call->parent)
    {
      call->parent->unexport (call->parent);
      _xdp_parent_free (call->parent);
    }
  g_free (call->parent_handle);

  if (call->signal_id)
    g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);

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
  OpenCall *call = data;
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
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "OpenURI canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "OpenURI failed");

  open_call_free (call);
}

static void do_open (OpenCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  OpenCall *call = data;
  call->parent_handle = g_strdup (handle);
  do_open (call);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  OpenCall *call = data;

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
}

#ifndef O_PATH
#define O_PATH 0
#endif

static void
do_open (OpenCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
  g_autoptr(GFile) file = NULL;
  GCancellable *cancellable;

  if (call->parent_handle == NULL)
    {
      call->parent->export (call->parent, parent_exported, call);
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
  g_variant_builder_add (&options, "{sv}", "writable", g_variant_new_boolean (call->writable));

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
          g_warning ("Failed to open '%s'", call->uri);
          return;
        }

      fd_list = g_unix_fd_list_new_from_array (&fd, 1);
      fd = -1;
      fd_in = 0;

      g_dbus_connection_call_with_unix_fd_list (call->portal->bus,
                                                PORTAL_BUS_NAME,
                                                PORTAL_OBJECT_PATH,
                                                "org.freedesktop.portal.OpenURI",
                                                "OpenFile",
                                                g_variant_new ("(sha{sv})", call->parent_handle, fd_in, &options),
                                                NULL,
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                fd_list,
                                                cancellable,
                                                NULL,
                                                NULL);
    }
  else
    {
      g_dbus_connection_call (call->portal->bus,
                              PORTAL_BUS_NAME,
                              PORTAL_OBJECT_PATH,
                              "org.freedesktop.portal.OpenURI",
                              "OpenUri",
                              g_variant_new ("(ssa{sv})", call->parent_handle, call->uri, &options),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              cancellable,
                              NULL,
                              NULL);
    }
}

/**
 * xdp_portal_open_uri:
 * @portal: a #XdpPortal
 * @parent: parent window information
 * @uri: the URI to open
 * @writable: whether to make the file writable
 * @cancellable: (nullable): optional #GCancellable
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Opens @uri with an external hamdler.
 */
void
xdp_portal_open_uri (XdpPortal           *portal,
                     XdpParent           *parent,
                     const char          *uri,
                     gboolean             writable,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             data)
{
  OpenCall *call = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  call = g_new0 (OpenCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = _xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->uri = g_strdup (uri);
  call->writable = writable;
  call->task = g_task_new (portal, cancellable, callback, data);

  do_open (call);
}

/**
 * xdp_portal_open_uri_finish:
 * @portal: a #XdpPortal
 * @result: a #GAsyncResult
 * @error: return location for an error
 *
 * Finishes the open-uri request, and returns
 * the result in the form of a boolean.
 *
 * Returns: %TRUE if the call succeeded
 */
gboolean
xdp_portal_open_uri_finish (XdpPortal *portal,
                            GAsyncResult *result,
                            GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

