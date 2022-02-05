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

#include "filechooser.h"
#include "portal-private.h"

typedef struct {
  XdpPortal *portal;
  XdpParent *parent;
  char *parent_handle;
  char *method;
  char *title;
  gboolean multiple;
  char *current_name;
  char *current_folder;
  char *current_file;
  GVariant *files;
  GVariant *filters;
  GVariant *current_filter;
  GVariant *choices;
  guint signal_id;
  GTask *task;
  char *request_path;
  guint cancelled_id;
} FileCall;

static void
file_call_free (FileCall *call)
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

  // call->method not free'ed as it is assigned from static strings
  g_free (call->title);
  g_free (call->current_name);
  g_free (call->current_folder);
  g_free (call->current_file);
  if (call->files)
    g_variant_unref (call->files);
  if (call->filters)
    g_variant_unref (call->filters);
  if (call->current_filter)
    g_variant_unref (call->current_filter);
  if (call->choices)
    g_variant_unref (call->choices);

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
  FileCall *call = data;
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
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Filechooser canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Filechooser failed");

  file_call_free (call);
}

static void open_file (FileCall *call);

static void
parent_exported (XdpParent *parent,
                 const char *handle,
                 gpointer data)
{
  FileCall *call = data;
  call->parent_handle = g_strdup (handle);
  open_file (call);
}

static void
cancelled_cb (GCancellable *cancellable,
              gpointer data)
{
  FileCall *call = data;

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

  g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "OpenFile call canceled by caller");

  file_call_free (call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  FileCall *call = data;
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
      file_call_free (call);
    }
}

static void
open_file (FileCall *call)
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
  if (call->multiple)
    g_variant_builder_add (&options, "{sv}", "multiple", g_variant_new_boolean (call->multiple));
  if (call->files)
    g_variant_builder_add (&options, "{sv}", "files", call->files);
  if (call->filters)
    g_variant_builder_add (&options, "{sv}", "filters", call->filters);
  if (call->current_filter)
    g_variant_builder_add (&options, "{sv}", "current_filter", call->current_filter);
  if (call->choices)
    g_variant_builder_add (&options, "{sv}", "choices", call->choices);
  if (call->current_name)
    g_variant_builder_add (&options, "{sv}", "current_name", g_variant_new_string (call->current_name));
  if (call->current_folder)
    g_variant_builder_add (&options, "{sv}", "current_folder", g_variant_new_bytestring (call->current_folder));
  if (call->current_file)
    g_variant_builder_add (&options, "{sv}", "current_file", g_variant_new_bytestring (call->current_file));

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.FileChooser",
                          call->method,
                          g_variant_new ("(ssa{sv})", call->parent_handle, call->title, &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          call_returned,
                          call);
}

/**
 * xdp_portal_open_file:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @title: title for the file chooser dialog
 * @filters: (nullable): a [struct@GLib.Variant] describing file filters
 * @current_filter: (nullable): a [struct@GLib.Variant] describing the current file filter
 * @choices: (nullable): a [struct@GLib.Variant] describing extra widgets
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Asks the user to open one or more files.
 *
 * The format for the @filters argument is `a(sa(us))`.
 * Each item in the array specifies a single filter to offer to the user.
 * The first string is a user-visible name for the filter. The `a(us)`
 * specifies a list of filter strings, which can be either a glob pattern
 * (indicated by 0) or a mimetype (indicated by 1).
 * 
 * Example: `[('Images', [(0, '*.ico'), (1, 'image/png')]), ('Text', [(0, '*.txt')])]`
 *
 * The format for the @choices argument is `a(ssa(ss)s)`.
 * For each element, the first string is an ID that will be returned
 * with the response, te second string is a user-visible label. The
 * `a(ss)` is the list of choices, each being a is an ID and a
 * user-visible label. The final string is the initial selection,
 * or `""`, to let the portal decide which choice will be initially selected.
 * None of the strings, except for the initial selection, should be empty.
 * 
 * As a special case, passing an empty array for the list of choices
 * indicates a boolean choice that is typically displayed as a check
 * button, using `"true"` and `"false"` as the choices.
 *
 * Example: `[('encoding', 'Encoding', [('utf8', 'Unicode (UTF-8)'), ('latin15', 'Western')], 'latin15'), ('reencode', 'Reencode', [], 'false')]`
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.open_file_finish] to get the results.
 */
void
xdp_portal_open_file (XdpPortal *portal,
                      XdpParent *parent,
                      const char *title,
                      GVariant *filters,
                      GVariant *current_filter,
                      GVariant *choices,
                      XdpOpenFileFlags flags,
                      GCancellable *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer data)
{
  FileCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail ((flags & ~(XDP_OPEN_FILE_FLAG_MULTIPLE)) == 0);

  call = g_new0 (FileCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->method = "OpenFile";
  call->title = g_strdup (title);
  call->multiple = (flags & XDP_OPEN_FILE_FLAG_MULTIPLE) != 0;
  call->filters = filters ? g_variant_ref (filters) : NULL;
  call->current_filter = current_filter ? g_variant_ref (current_filter) : NULL;
  call->choices = choices ? g_variant_ref (choices) : NULL;
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_open_file);

  open_file (call);
}

/**
 * xdp_portal_open_file_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the open-file request
 *
 * Returns the result in the form of a [struct@GLib.Variant] dictionary
 * containing the following fields:
 *
 * - uris `as`: an array of strings containing the uris of selected files
 * - choices `a(ss)`: an array of pairs of strings, the first string being the
 *     ID of a combobox that was passed into this call, the second string
 *     being the selected option.
 *
 * Returns: (transfer full): a [struct@GLib.Variant] dictionary with the results
 */
GVariant *
xdp_portal_open_file_finish (XdpPortal *portal,
                             GAsyncResult *result,
                             GError **error)
{
  GVariant *ret;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_open_file, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);
  return ret ? g_variant_ref (ret) : NULL;
}

/**
 * xdp_portal_save_file:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @title: title for the file chooser dialog
 * @current_name: (nullable): suggested filename
 * @current_folder: (nullable): suggested folder to save the file in
 * @current_file: (nullable): the current file (when saving an existing file)
 * @filters: (nullable): a [struct@GLib.Variant] describing file filters
 * @current_filter: (nullable): a [struct@GLib.Variant] describing the current file filter
 * @choices: (nullable): a [struct@GLib.Variant] describing extra widgets
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Asks the user for a location to save a file.
 *
 * The format for the @filters argument is the same as for [method@Portal.open_file].

 * The format for the @choices argument is the same as for [method@Portal.open_file].
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.save_file_finish] to get the results.
 */
void
xdp_portal_save_file (XdpPortal *portal,
                      XdpParent *parent,
                      const char *title,
                      const char *current_name,
                      const char *current_folder,
                      const char *current_file,
                      GVariant *filters,
                      GVariant *current_filter,
                      GVariant *choices,
                      XdpSaveFileFlags flags,
                      GCancellable *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer data)
{
  FileCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_SAVE_FILE_FLAG_NONE);

  call = g_new0 (FileCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->method = "SaveFile";
  call->title = g_strdup (title);
  call->current_name = g_strdup (current_name);
  call->current_folder = g_strdup (current_folder);
  call->current_file = g_strdup (current_file);
  call->filters = filters ? g_variant_ref (filters) : NULL;
  call->current_filter = current_filter ? g_variant_ref (current_filter) : NULL;
  call->choices = choices ? g_variant_ref (choices) : NULL;
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_save_file);

  open_file (call);
}

/**
 * xdp_portal_save_file_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the save-file request.
 *
 * Returns the result in the form of a [struct@GLib.Variant] dictionary
 * containing the following fields:
 *
 * - uris `(as)`: an array of strings containing the uri of the selected file
 * - choices `a(ss)`: an array of pairs of strings, the first string being the
 *   ID of a combobox that was passed into this call, the second string
 *   being the selected option.
 *
 * Returns: (transfer full): a [struct@GLib.Variant] dictionary with the results
 */
GVariant *
xdp_portal_save_file_finish (XdpPortal *portal,
                             GAsyncResult *result,
                             GError **error)
{
  GVariant *ret;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_save_file, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);
  return ret ? g_variant_ref (ret) : NULL;
}

/**
 * xdp_portal_save_files:
 * @portal: a [class@Portal]
 * @parent: (nullable): parent window information
 * @title: title for the file chooser dialog
 * @current_name: (nullable): suggested filename
 * @current_folder: (nullable): suggested folder to save the file in
 * @files: An array of file names to be saved
 * @choices: (nullable): a [struct@GLib.Variant] describing extra widgets
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Asks for a folder as a location to save one or more files.
 *
 * The names of the files will be used as-is and appended to the selected
 * folder's path in the list of returned files. If the selected folder already
 * contains a file with one of the given names, the portal may prompt or take
 * some other action to construct a unique file name and return that instead.
 *
 * The format for the @choices argument is the same as for [method@Portal.open_file].
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.save_file_finish] to get the results.
 */
void
xdp_portal_save_files (XdpPortal *portal,
                       XdpParent *parent,
                       const char *title,
                       const char *current_name,
                       const char *current_folder,
                       GVariant *files,
                       GVariant *choices,
                       XdpSaveFileFlags flags,
                       GCancellable *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer data)
{
  FileCall *call;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (files != NULL);
  g_return_if_fail (flags == XDP_SAVE_FILE_FLAG_NONE);

  call = g_new0 (FileCall, 1);
  call->portal = g_object_ref (portal);
  if (parent)
    call->parent = xdp_parent_copy (parent);
  else
    call->parent_handle = g_strdup ("");
  call->method = "SaveFiles";
  call->title = g_strdup (title);
  call->current_name = g_strdup (current_name);
  call->current_folder = g_strdup (current_folder);
  call->files = g_variant_ref (files);
  call->choices = choices ? g_variant_ref (choices) : NULL;
  call->task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (call->task, xdp_portal_save_files);

  open_file (call);
}

/**
 * xdp_portal_save_files_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the save-files request.
 *
 * Returns the result in the form of a [struct@GLib.Variant] dictionary
 * containing the following fields:
 *
 * - uris `(as)`: an array of strings containing the uri corresponding to each
 *   file passed to the save-files request, in the same order. Note that the
 *   file names may have changed, for example if a file with the same name in
 *   the selected folder already exists.
 * - choices `a(ss)`: an array of pairs of strings, the first string being the
 *   ID of a combobox that was passed into this call, the second string
 *   being the selected option.
 *
 * Returns: (transfer full): a [struct@GLib.Variant] dictionary with the results
 */
GVariant *
xdp_portal_save_files_finish (XdpPortal *portal,
                              GAsyncResult *result,
                              GError **error)
{
  GVariant *ret;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_save_files, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);
  return ret ? g_variant_ref (ret) : NULL;
}
