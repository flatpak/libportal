/*
 * Copyright (C) 2025 Red Hat
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

#include <gio/gunixfdlist.h>

#include "clipboard.h"
#include "portal-private.h"
#include "session-private.h"

static void
on_selection_owner_changed (GDBusConnection *bus,
                            const char *sender_name,
                            const char *object_path,
                            const char *interface_name,
                            const char *signal_name,
                            GVariant *parameters,
                            gpointer data)
{
  XdpPortal *portal = data;
  XdpSession *session;
  g_autoptr(GVariant) variant = NULL;
  const char *session_handle = NULL;
  gboolean session_is_owner = FALSE;
  g_autofree const char **mime_types = NULL;

  g_variant_get (parameters, "(o@a{sv})", &session_handle, &variant);

  session = xdp_portal_lookup_session (portal, session_handle);
  if (!session)
    return;

  g_variant_lookup (variant, "session_is_owner", "b", &session_is_owner);
  g_variant_lookup (variant, "mime_types", "^a&s", &mime_types);

  session->is_selection_owned_by_session = session_is_owner;
  g_clear_pointer (&session->selection_mime_types, g_strfreev);
  session->selection_mime_types = g_strdupv ((GStrv) mime_types);

  g_signal_emit_by_name (session, "selection-owner-changed",
                         mime_types, session_is_owner);
}

static void
on_selection_transfer (GDBusConnection *bus,
                       const char *sender_name,
                       const char *object_path,
                       const char *interface_name,
                       const char *signal_name,
                       GVariant *parameters,
                       gpointer data)
{
  XdpPortal *portal = data;
  XdpSession *session;
  g_autoptr(GVariant) variant = NULL;
  const char *session_handle = NULL;
  const char *mime_type = NULL;
  unsigned int serial = 0;

  g_variant_get (parameters, "(osu)", &session_handle, &mime_type, &serial);

  session = xdp_portal_lookup_session (portal, session_handle);
  if (!session)
    return;

  g_signal_emit_by_name (session, "selection-transfer",
                         mime_type, serial);
}

void
ensure_signal_listeners (XdpPortal *portal)
{
  if (portal->selection_owner_changed_signal)
    return;

  portal->selection_owner_changed_signal =
    g_dbus_connection_signal_subscribe (portal->bus,
                                        PORTAL_BUS_NAME,
                                        "org.freedesktop.portal.Clipboard",
                                        "SelectionOwnerChanged",
                                        PORTAL_OBJECT_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        on_selection_owner_changed,
                                        portal,
                                        NULL);
  portal->selection_transfer_signal =
    g_dbus_connection_signal_subscribe (portal->bus,
                                        PORTAL_BUS_NAME,
                                        "org.freedesktop.portal.Clipboard",
                                        "SelectionTransfer",
                                        PORTAL_OBJECT_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        on_selection_transfer,
                                        portal,
                                        NULL);
}

/**
 * xdp_session_request_clipboard:
 * @session: a [class@Session] in initial state
 *
 * Requests clipboard integration on the session.
 */
void
xdp_session_request_clipboard (XdpSession *session)
{
  GVariantBuilder options;

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_dbus_connection_call (session->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Clipboard",
                          "RequestClipboard",
                          g_variant_new ("(oa{sv})",
                                         session->id,
                                         &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, NULL, NULL);

  ensure_signal_listeners (session->portal);
}

/**
 * xdp_session_is_clipboard_enabed:
 * @session: a [class@Session]
 *
 * Returns TRUE if the session has enabled clipboard integration.
 */
gboolean
xdp_session_is_clipboard_enabled (XdpSession *session)
{
  return session->is_clipboard_enabled;
}

/**
 * xdp_session_is_selection_owned_by_session:
 * @session: a [class@Session]
 *
 * Return TRUE if the most recently received clipboard selection owner is the
 * selection of this session.
 */
gboolean
xdp_session_is_selection_owned_by_session (XdpSession *session)
{
  return session->is_selection_owned_by_session;
}

/**
 * xdp_session_get_selection_mime_types:
 * @session: a [class@Session]
 *
 * Get the currently advertised mime types of the current clipboard selection
 * owner.
 *
 * Returns: (transfer none): A NULL terminated array of mime type strings.
 */
const char **
xdp_session_get_selection_mime_types (XdpSession *session)
{
  return (const char **) session->selection_mime_types;
}

/**
 * xdp_session_set_selection:
 * @session: a [class@Session]
 * @mime_types: (transfer none): A NULL terminated array of mime type strings.
 *
 * Set the clipboard selection to advertise support for the passed list of mime
 * types.
 *
 * When some entity in the windowing system requests to retrieve the contents of
 * the selection, the [signal@Session::selection-transfer] signal is emitted. In
 * response to this, the caller of this function must respond by calling
 * [method@Session.selection_write] with the serial number passed via the
 * mentioned signal.
 */
void
xdp_session_set_selection (XdpSession *session,
                           const char **mime_types)
{
  GVariantBuilder mime_types_builder;
  GVariantBuilder options;
  int i = 0;

  g_variant_builder_init (&mime_types_builder, G_VARIANT_TYPE ("as"));
  for (i = 0; mime_types[i]; i++)
    g_variant_builder_add (&mime_types_builder, "s", mime_types[i]);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}",
                         "mime_types", g_variant_builder_end (&mime_types_builder));
  g_dbus_connection_call (session->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Clipboard",
                          "SetSelection",
                          g_variant_new ("(oa{sv})",
                                         session->id,
                                         &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, NULL, NULL);
}

/**
 * xdp_session_selection_write:
 * @session: a [class@Session]
 * @serial: a serial number
 *
 * Retrieve a file descriptor to write the content of the clipboard selection
 * to. The content should be formatted according to the mime type of the mime
 * type passed via the [signal@Session::selection-transfer] that carried the
 * passed serial number.
 *
 * Returns: a file descriptor to a pipe to write the clipboard
 * selection content to, or -1 on error. The caller must close the file
 * descriptor when finished with it.
 */
int
xdp_session_selection_write (XdpSession *session,
                             unsigned int serial)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  int fd_out;

  ret =
    g_dbus_connection_call_with_unix_fd_list_sync (session->portal->bus,
                                                   PORTAL_BUS_NAME,
                                                   PORTAL_OBJECT_PATH,
                                                   "org.freedesktop.portal.Clipboard",
                                                   "SelectionWrite",
                                                   g_variant_new ("(ou)",
                                                                  session->id,
                                                                  serial),
                                                   G_VARIANT_TYPE ("(h)"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &fd_list,
                                                   NULL,
                                                   &error);
  if (!ret)
    {
      g_warning ("Failed to write selection: %s", error->message);
      return -1;
    }

  g_variant_get (ret, "(h)", &fd_out);

  return g_unix_fd_list_get (fd_list, fd_out, NULL);
}

/**
 * xdp_session_selection_write_done:
 * @session: a [class@Session]
 * @serial: a serial number
 * @success: TRUE if the transfer was successful
 *
 * Notify whether a clipboard selection write operation associated with the
 * passed serial was successful or not.
 */
void
xdp_session_selection_write_done (XdpSession   *session,
                                  unsigned int  serial,
                                  gboolean      success)
{
  g_dbus_connection_call (session->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Clipboard",
                          "SetSelection",
                          g_variant_new ("(oub)",
                                         session->id,
                                         serial,
                                         success),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, NULL, NULL);
}

/**
 * xdp_session_selection_read:
 * @session: a [class@Session]
 * @mime_type: a string containing the requested mime type
 *
 * Request to read the contents of the current clipboard selection in the passed
 * mime type format. On success a file descriptor is returned, which can be read
 * from to retrieve the clipboard selection content.
 *
 * Returns: A file descriptor, or -1 on error. The caller must close the file
 * descriptor when finished with it.
 */
int
xdp_session_selection_read (XdpSession *session,
                            const char *mime_type)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  int fd_out;

  ret =
    g_dbus_connection_call_with_unix_fd_list_sync (session->portal->bus,
                                                   PORTAL_BUS_NAME,
                                                   PORTAL_OBJECT_PATH,
                                                   "org.freedesktop.portal.Clipboard",
                                                   "SelectionRead",
                                                   g_variant_new ("(os)",
                                                                  session->id,
                                                                  mime_type),
                                                   G_VARIANT_TYPE ("(h)"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &fd_list,
                                                   NULL,
                                                   &error);
  if (!ret)
    {
      g_warning ("Failed to write selection: %s", error->message);
      return -1;
    }

  g_variant_get (ret, "(h)", &fd_out);

  return g_unix_fd_list_get (fd_list, fd_out, NULL);
}
