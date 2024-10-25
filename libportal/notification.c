/*
 * Copyright (C) 2018, Matthias Clasen
 * Copyright (C) 2024 GNOME Foundation, Inc.
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
 *
 * Authors:
 *    Matthias Clasen <mclasen@redhat.com>
 *    Julian Sparber <julian@gnome.org>
 */

#include "config.h"

#define _GNU_SOURCE
#include <sys/mman.h>
#include <fcntl.h>

#include <glib/gstdio.h>
#include <gio/gunixfdlist.h>
#include <gio/gunixoutputstream.h>

#include "notification.h"
#include "portal-private.h"

static void
action_invoked (GDBusConnection *bus,
                const char *sender_name,
                const char *object_path,
                const char *interface_name,
                const char *signal_name,
                GVariant *parameters,
                gpointer data)
{
  XdpPortal *portal = data;
  const char *id;
  const char *action;
  g_autoptr(GVariant) parameter = NULL;

  g_variant_get (parameters, "(&s&s@av)", &id, &action, &parameter);

  g_signal_emit_by_name (portal, "notification-action-invoked",
                         id, action, parameter);
}

static void
ensure_action_invoked_connection (XdpPortal *portal)
{
  if (portal->action_invoked_signal == 0)
    portal->action_invoked_signal =
       g_dbus_connection_signal_subscribe (portal->bus,
                                           PORTAL_BUS_NAME,
                                           "org.freedesktop.portal.Notification",
                                           "ActionInvoked",
                                           PORTAL_OBJECT_PATH,
                                           NULL,
                                           G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                           action_invoked,
                                           portal,
                                           NULL);
}

typedef struct {
  GUnixFDList *fd_list;
  GVariantBuilder *builder;
  gsize hold_count;
} ParserData;

static ParserData*
parser_data_new (const char *id)
{
  ParserData *data;

  data = g_new0 (ParserData, 1);
  data->fd_list = g_unix_fd_list_new ();
  data->builder = g_variant_builder_new (G_VARIANT_TYPE ("(sa{sv})"));

  g_variant_builder_add (data->builder, "s", id);
  g_variant_builder_open (data->builder, G_VARIANT_TYPE ("a{sv}"));

  return data;
}

static void
parser_data_hold (ParserData *data)
{
  data->hold_count++;
}

static gboolean
parser_data_release (ParserData *data)
{
  data->hold_count--;

  if (data->hold_count == 0)
    g_variant_builder_close (data->builder);

  return data->hold_count == 0;
}

static void
parser_data_free (gpointer user_data)
{
  ParserData *data = user_data;

  g_clear_object (&data->fd_list);
  g_clear_pointer (&data->builder, g_variant_builder_unref);

  g_free (data);
}

static void
call_done_cb (GObject      *source,
              GAsyncResult *result,
              gpointer      data)
{
  g_autoptr(GTask) task = G_TASK (data);
  XdpPortal *portal = g_task_get_source_object (task);
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) res = NULL;

  res = g_dbus_connection_call_with_unix_fd_list_finish (portal->bus, NULL, result, &error);

  if (res)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static void
parse_buttons_v1 (GVariantBuilder *builder,
                  GVariant        *value)
{
  GVariant *button;
  GVariantIter iter_buttons;

  g_variant_builder_open (builder, G_VARIANT_TYPE ("{sv}"));
  g_variant_builder_add (builder, "s", "buttons");
  g_variant_builder_open (builder, G_VARIANT_TYPE_VARIANT);
  g_variant_builder_open (builder, G_VARIANT_TYPE ("aa{sv}"));
  g_variant_iter_init (&iter_buttons, value);
  while (g_variant_iter_loop (&iter_buttons, "@a{sv}", &button))
    {
      GVariant *button_value;
      const gchar *button_key;
      GVariantIter iter_button;
      g_variant_builder_open (builder, G_VARIANT_TYPE ("a{sv}"));

      g_variant_iter_init (&iter_button, button);
      while (g_variant_iter_loop (&iter_button, "{&sv}", &button_key, &button_value))
        {
          if (strcmp (button_key, "purpose") != 0)
            g_variant_builder_add (builder, "{sv}", button_key, button_value);
        }

      g_variant_builder_close (builder);
    }

  g_variant_builder_close (builder);
  g_variant_builder_close (builder);
  g_variant_builder_close (builder);
}

static void
parse_notification (GVariant            *notification,
                    uint                 version,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  GVariantIter iter;
  const char *id;
  const char *key;
  GVariant *value = NULL;
  GVariant *properties = NULL;
  ParserData *data;
  g_autoptr(GTask) task;
  const char *supported_properties_v1[] = {
    "title",
    "body",
    "buttons",
    "icon",
    "priority",
    "default-action",
    "default-action-target",
    NULL
  };

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, parse_notification);

  g_variant_get (notification, "(&s@a{sv})", &id, &properties);

  data = parser_data_new (id);
  g_task_set_task_data (task,
                        data,
                        parser_data_free);

  g_variant_iter_init (&iter, properties);

  parser_data_hold (data);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
      /* In version 1 of the interface requests fail if there are unsupported properties */
      if (version < 2)
        {
          if (!g_strv_contains (supported_properties_v1, key))
            continue;

          if (strcmp (key, "buttons") == 0)
            {
              parse_buttons_v1 (data->builder, value);
              continue;
            }
        }

      g_variant_builder_add (data->builder, "{sv}", key, value);
    }

  if (parser_data_release (data))
    {
      g_task_return_boolean (task, TRUE);
    }
}

static GVariant *
parse_notification_finish (GAsyncResult  *result,
                           GUnixFDList  **fd_list,
                           GError       **error)
{
  ParserData *data;
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == parse_notification, NULL);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return NULL;

  data = g_task_get_task_data (G_TASK (result));

  if (fd_list)
    *fd_list = g_steal_pointer (&data->fd_list);

  return g_variant_ref_sink (g_variant_builder_end (g_steal_pointer (&data->builder)));
}

static void
parse_notification_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(GTask) task = G_TASK (user_data);
  XdpPortal *portal = g_task_get_source_object (task);
  g_autoptr(GError) error = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GVariant) parameters = NULL;

  parameters = parse_notification_finish (result, &fd_list, &error);

  if (!parameters)
    {
        g_task_return_error (task, g_steal_pointer (&error));
        return;
    }

  g_dbus_connection_call_with_unix_fd_list (portal->bus,
                                            PORTAL_BUS_NAME,
                                            PORTAL_OBJECT_PATH,
                                            "org.freedesktop.portal.Notification",
                                            "AddNotification",
                                            parameters,
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            fd_list,
                                            g_task_get_cancellable (task),
                                            call_done_cb,
                                            g_object_ref (task));
}



static void
get_properties_cb (GObject      *source_object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr(GTask) task = G_TASK (user_data);
  XdpPortal *portal = g_task_get_source_object (task);
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariant) vardict = NULL;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), result, &error);
  if (!ret)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_variant_get (ret, "(@a{sv})", &vardict);

  if (!g_variant_lookup (vardict, "version", "u", &portal->notification_interface_version))
    portal->notification_interface_version = 1;

  g_task_return_boolean (task, TRUE);
}

static void
get_supported_features (XdpPortal *portal,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
  g_autoptr(GTask) task = NULL;

  task = g_task_new (portal, cancellable, callback, user_data);
  g_task_set_source_tag (task, get_supported_features);

  if (portal->notification_interface_version != 0)
    {
      g_task_return_boolean (task, TRUE);
      return;
    }

  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.DBus.Properties",
                          "GetAll",
                          g_variant_new ("(s)", "org.freedesktop.portal.Notification"),
                          G_VARIANT_TYPE ("(a{sv})"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          get_properties_cb,
                          g_object_ref (task));
}

static gboolean
get_supported_features_finish (XdpPortal     *portal,
                               GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == get_supported_features, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
get_supported_features_cb (GObject      *source_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  XdpPortal *portal = XDP_PORTAL (source_object);
  g_autoptr(GTask) task = G_TASK (user_data);
  g_autoptr(GError) error = NULL;

  if (!get_supported_features_finish (portal, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  parse_notification (g_task_get_task_data (task),
                      portal->notification_interface_version,
                      g_task_get_cancellable (task),
                      parse_notification_cb,
                      g_object_ref (task));
}

/**
 * xdp_portal_add_notification:
 * @portal: a [class@Portal]
 * @id: unique ID for the notification
 * @notification: a [struct@GLib.Variant] dictionary with the content of the notification
 * @flags: options for this call
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: (closure): data to pass to @callback
 *
 * Sends a desktop notification.
 *
 * The following keys may be present in @notification:
 *
 * - title `s`: a user-visible string to display as title
 * - body `s`: a user-visible string to display as body
 * - icon `v`: a serialized icon (in the format produced by [method@Gio.Icon.serialize])
 * - priority `s`: "low", "normal", "high" or "urgent"
 * - default-action `s`: name of an action that
 *     will be activated when the user clicks on the notification
 * - default-action-target `v`: target parameter to send along when
 *     activating the default action.
 * - buttons `aa{sv}`: array of serialized buttons
 *
 * Each serialized button is a dictionary with the following supported keys:
 *
 * - label `s`: user-visible lable for the button. Mandatory
 * - action `s`: name of an action that will be activated when
 *     the user clicks on the button. Mandatory
 * - target `v`: target parameter to send along when activating
 *     the button
 *
 * Actions with a prefix of "app." are assumed to be exported by the
 * application and will be activated via the org.freedesktop.Application
 * interface, others are activated by emitting the
 * [signal@Portal::notification-action-invoked] signal.
 *
 * It is the callers responsibility to ensure that the ID is unique
 * among all notifications.
 *
 * To withdraw a notification, use [method@Portal.remove_notification].
 */
void
xdp_portal_add_notification (XdpPortal *portal,
                             const char *id,
                             GVariant *notification,
                             XdpNotificationFlags flags,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GVariant) variant = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));
  g_return_if_fail (flags == XDP_NOTIFICATION_FLAG_NONE);

  ensure_action_invoked_connection (portal);

  variant = g_variant_ref_sink (g_variant_new ("(s@a{sv})", id, notification));
  task = g_task_new (portal, cancellable, callback, data);
  g_task_set_source_tag (task, xdp_portal_add_notification);
  g_task_set_task_data (task, g_steal_pointer (&variant), (GDestroyNotify) g_variant_unref);

  get_supported_features (portal,
                          cancellable,
                          get_supported_features_cb,
                          g_steal_pointer (&task));
}

/**
 * xdp_portal_add_notification_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the notification request.
 *
 * Returns the result as a boolean.
 *
 * Returns: `TRUE` if the notification was added
 */
gboolean
xdp_portal_add_notification_finish (XdpPortal     *portal,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  g_return_val_if_fail (XDP_IS_PORTAL (portal), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, portal), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == xdp_portal_add_notification, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * xdp_portal_remove_notification:
 * @portal: a [class@Portal]
 * @id: the ID of an notification
 *
 * Withdraws a desktop notification.
 */
void
xdp_portal_remove_notification (XdpPortal  *portal,
                                const char *id)
{
  g_return_if_fail (XDP_IS_PORTAL (portal));

  g_dbus_connection_call (portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.Notification",
                          "RemoveNotification",
                          g_variant_new ("(s)", id),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}
