/*
 * Copyright (C) 2018, Matthias Clasen
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

#include "print.h"
#include "portal-private.h"

#define GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>

#ifndef O_PATH
#define O_PATH 0
#endif

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  char *title;
  gboolean is_prepare;
  GVariant *settings;
  GVariant *page_setup;
  guint token;
  char *file;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;
} PrintCall;

static void
print_call_free (PrintCall *call)
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

  g_free (call->title);
  if (call->settings)
    g_variant_unref (call->settings);
  if (call->page_setup)
    g_variant_unref (call->page_setup);
  g_free (call->file);

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
  PrintCall *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  if (call->cancelled_id)
    {
      g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
      call->cancelled_id = 0;
    }

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    {
      if (call->is_prepare)
        g_task_return_pointer (call->task, g_variant_ref (ret), (GDestroyNotify)g_variant_unref);
      else
        g_task_return_boolean (call->task, TRUE);
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Print canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Print failed");

  print_call_free (call);
}

static void do_print (PrintCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  PrintCall *call = data;
  call->parent_handle = g_strdup (handle);
  do_print (call);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  PrintCall *call = data;

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

  g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Print call canceled by caller");

  print_call_free (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  PrintCall *call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;

  if (call->is_prepare)
    ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  else
    ret = g_dbus_connection_call_with_unix_fd_list_finish (G_DBUS_CONNECTION (object), NULL, result, &error);
  if (error)
    {
      if (call->cancelled_id)
        {
          g_signal_handler_disconnect (g_task_get_cancellable (call->task), call->cancelled_id);
          call->cancelled_id = 0;
        }

      g_task_return_error (call->task, error);
      print_call_free (call);
    }
}

static void
do_print (PrintCall *call)
{
  GVariantBuilder options;
  g_autofree char *token = NULL;
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
  if (!call->is_prepare)
    g_variant_builder_add (&options, "{sv}", "token", g_variant_new_uint32 (call->token));
  
  if (call->is_prepare)
    g_dbus_connection_call (call->portal->bus,
                            PORTAL_BUS_NAME,
                            PORTAL_OBJECT_PATH,
                            "org.freedesktop.portal.Print",
                            "PreparePrint",
                            g_variant_new ("(ss@a{sv}@a{sv}a{sv})",
                                           call->parent_handle,
                                           call->title,
                                           call->settings ? call->settings : g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0),
                                           call->page_setup ? call->page_setup : g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0),
                                           &options),
                            NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            call_returned,
                            call);
  else
    {
      g_autoptr(GUnixFDList) fd_list = NULL;
      int fd, fd_in;

      fd = g_open (call->file, O_PATH | O_CLOEXEC);
      if (fd == -1)
        {
          g_warning ("Failed to open '%s'", call->file);
          return;
        }

      fd_list = g_unix_fd_list_new_from_array (&fd, 1);
      fd = -1;
      fd_in = 0;

      g_dbus_connection_call_with_unix_fd_list (call->portal->bus,
                                                PORTAL_BUS_NAME,
                                                PORTAL_OBJECT_PATH,
                                                "org.freedesktop.portal.Print",
                                                "Print",
                                                g_variant_new ("(ssha{sv})",
                                                               call->parent_handle,
                                                               call->title,
                                                               fd_in,
                                                               &options),
                                                NULL,
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                fd_list,
                                                cancellable,
                                                call_returned,
                                                call);
    }
}

/**
 * xdp_portal_prepare_print:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @title: tile for the print dialog
 * @settings: (nullable): Serialized print settings
 * @page_setup: (nullable): Serialized page setup
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 * 
 * Presents a print dialog to the user and returns print settings and page setup.
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.prepare_print_finish] to get the results.
 */
void
xdp_portal_prepare_print (XdpPortal *portal,
                          XdpParent *parent,
                          const char *title,
                          GVariant *settings,
                          GVariant *page_setup,
                          XdpPrintFlags flags,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer data)
{
  PrintCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_PRINT_FLAG_NONE);

  call = g_new0 (PrintCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->title = g_strdup (title);
  call->is_prepare = TRUE;
  call->settings = settings ? g_variant_ref (settings) : NULL;
  call->page_setup = page_setup ? g_variant_ref (page_setup) : NULL;
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_prepare_print);

  do_print (call);
}

/**
 * xdp_portal_prepare_print_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the prepare-print request.
 *
 * Returns a [struct@GLib.Variant] dictionary with the following information:
 *
 * - settings `a{sv}`: print settings as set up by the user in the print dialog
 * - page-setup `a{sv}: page setup as set up by the user in the print dialog
 * - token u: a token that can by used in a [method@Portal.print_file] call to
 *     avoid the print dialog
 *
 * Returns: (transfer full): a [struct@GLib.Variant] dictionary with print information
 */
GVariant *
xdp_portal_prepare_print_finish (XdpPortal *portal,
                                 GAsyncResult *result,
                                 GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_prepare_print, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * xdp_portal_print_file:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @title: tile for the print dialog
 * @token: token that was returned by a previous [method@Portal.prepare_print] call, or 0
 * @file: path of the document to print
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 * 
 * Prints a file.
 *
 * If a valid token is present in the @options, then this call will print
 * with the settings from the Print call that the token refers to. If
 * no token is present, then a print dialog will be presented to the user.
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.print_file_finish] to get the results.
 */
void
xdp_portal_print_file (XdpPortal *portal,
                       XdpParent *parent,
                       const char *title,
                       guint token,
                       const char *file,
                       XdpPrintFlags flags,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer data)
{
  PrintCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_PRINT_FLAG_NONE);

  call = g_new0 (PrintCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->title = g_strdup (title);
  call->is_prepare = FALSE;
  call->token = token;
  call->file = g_strdup (file);
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_print_file);

  do_print (call);
}

/**
 * xdp_portal_print_file_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the print request.
 *
 * Returns: `TRUE` if the request was successful
 */
gboolean
xdp_portal_print_file_finish (XdpPortal *portal,
                              GAsyncResult *result,
                              GError **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_print_file, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
