/*
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
 *    Hubert Figuière <hub@figuiere.net>
 */

#include "config.h"

#include "portal.h"
#include "settings.h"

#include "portal-private.h"

/**
 * XdpSettings
 *
 * A representation of the settings exposed by the portal.
 *
 * The [class@Settings] object is used to access and observe the settings
 * exposed by xdg-desktop-portal.
 *
 * It is obtained from [method@Portal.get_settings]. Call
 * [method@Settings.read_value] to read a settings value. Connect to
 * [signal@Settings::changed] to observe value changes.
 */
struct _XdpSettings {
  GObject parent_instance;

  XdpPortal *portal;
  guint signal_id;

};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (XdpSettings, xdp_settings, G_TYPE_OBJECT)

static void
xdp_settings_finalize (GObject *object)
{
  XdpSettings *settings = XDP_SETTINGS (object);

  if (settings->signal_id)
    g_dbus_connection_signal_unsubscribe (settings->portal->bus, settings->signal_id);

  g_clear_object (&settings->portal);

  G_OBJECT_CLASS (xdp_settings_parent_class)->finalize (object);
}

static void
xdp_settings_class_init (XdpSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_settings_finalize;

  /**
   * XdpSettings::changed:
   * @settings: the [class@Settings] object
   * @namespace: the value namespace
   * @key: the value key
   * @value: the value
   *
   * Emitted when a setting value is changed externally.
   */
  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_VARIANT);
}

static void
xdp_settings_init (XdpSettings *settings)
{
}

static void
settings_changed (GDBusConnection *bus,
		  const char *sender_name,
		  const char *object_path,
		  const char *interface_name,
		  const char *signal_name,
		  GVariant *parameters,
		  gpointer data)
{
  const char *namespace = NULL;
  const char *key = NULL;
  g_autoptr(GVariant) value = NULL;
  guint n_params;

  XdpSettings *settings = data;

  n_params = g_variant_n_children (parameters);
  if (n_params != 3)
    {
      g_warning ("Incorrect number of parameters, expected 3, got %u", n_params);
      return;
    }

  g_variant_get_child (parameters, 0, "&s", &namespace);
  g_variant_get_child (parameters, 1, "&s", &key);
  g_variant_get_child (parameters, 2, "v", &value);

  g_signal_emit (settings, signals[CHANGED], 0, namespace, key, value);
}

/**
 * xdp_settings_read_uint:
 * @settings: the [class@Settings] object.
 * @namespace: the namespace of the value.
 * @key: the key of the value.
 * @cancellable: a GCancellable or NULL.
 * @error: return location for error or NULL.
 *
 * Read a setting value as unsigned int within @namespace, with @key.
 *
 * Returns: the uint value, or 0 if not found or not
 * the right type. If @error is not NULL, then the error is returned.
 */
guint32
xdp_settings_read_uint (XdpSettings *settings, const char *namespace, const char *key, GCancellable *cancellable, GError **error)
{
  g_autoptr(GVariant) value = NULL;

  value = xdp_settings_read_value (settings, namespace, key, cancellable, error);
  if (value)
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_UINT32))
        return g_variant_get_uint32 (value);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Value doesn't contain an integer.");
    }

  return 0;
}

/**
 * xdp_settings_read_string:
 * @settings: the [class@Settings] object.
 * @namespace: the namespace of the value.
 * @key: the key of the value.
 * @cancellable: a GCancellable or NULL.
 * @error: return location for error or NULL.
 *
 * Read a setting value as unsigned int within @namespace, with @key.
 *
 * Returns: (transfer full): the stringint value, or NULL if not found or not
 * the right type. If @error is not NULL, then the error is returned.
 */
char *
xdp_settings_read_string (XdpSettings *settings, const char *namespace, const char *key, GCancellable *cancellable, GError **error)
{
  g_autoptr(GVariant) value = NULL;

  value = xdp_settings_read_value (settings, namespace, key, cancellable, error);
  if (value)
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        return g_variant_dup_string (value, NULL);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Value doesn't contain a string.");
    }

  return NULL;
}

/**
 * xdp_settings_read:
 * @settings: the [class@Settings] object.
 * @namespace: the namespace of the value.
 * @key: the key of the value.
 * @cancellable: a GCancellable or NULL.
 * @error: return location for error or NULL.
 * @format: a #GVariant format string
 * @...: arguments as per @format
 *
 * Read a setting value within @namespace, with @key.
 *
 * A convenience function that combines xdp_settings_read_value() with
 * g_variant_get().
 *
 * In case of error, if @error is not NULL, then the error is
 * returned.
 */
void
xdp_settings_read (XdpSettings *settings, const char *namespace, const gchar *key, GCancellable *cancellable, GError **error, const gchar *format, ...)
{
  g_autoptr(GVariant) value = NULL;
  va_list ap;

  value = xdp_settings_read_value (settings, namespace, key, cancellable, error);
  if (value)
    {
      va_start (ap, format);
      g_variant_get_va (value, format, NULL, &ap);
      va_end (ap);
    }
}

/**
 * xdp_settings_read_value:
 * @settings: the [class@Settings] object.
 * @namespace: the namespace of the value.
 * @key: the key of the value.
 * @cancellable: a GCancellable or NULL.
 * @error: return location for error or NULL.
 *
 * Read a setting value within @namespace, with @key.
 *
 * Returns: (transfer full): the value, or %NULL if not
 * found. If @error is not NULL, then the error is returned.
 */
GVariant *
xdp_settings_read_value (XdpSettings *settings, const char *namespace, const char *key, GCancellable *cancellable, GError **error)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariant) inner = NULL;

  ret = g_dbus_connection_call_sync (settings->portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     SETTINGS_INTERFACE,
                                     "ReadOne",
                                     g_variant_new ("(ss)", namespace, key),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     5000,
                                     cancellable, error);
  if (!ret)
    return NULL;

  g_variant_get (ret, "(v)", &inner);

  return g_steal_pointer (&inner);
}

/**
 * xdp_settings_read_all_values:
 * @settings: the [class@Settings] object.
 * @namespaces: List of namespaces to filter results by, supports
 * simple globbing explained below.
 * @cancellable: a GCancellable or NULL.
 * @error: return location for error or NULL.
 *
 * Read all the setting values within @namespace.
 *
 * Returns: (transfer full): a value containing all the values, or
 * %NULL if not found. If @error is not NULL, then the error is
 * returned.
 */
GVariant *
xdp_settings_read_all_values (XdpSettings *settings, const char *const *namespaces, GCancellable *cancellable, GError **error)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariant) inner = NULL;

  ret = g_dbus_connection_call_sync (settings->portal->bus,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     SETTINGS_INTERFACE,
                                     "ReadAll",
                                     g_variant_new ("(^as)", namespaces),
                                     NULL,
                                     G_DBUS_CALL_FLAGS_NONE,
                                     5000,
                                     cancellable, error);
  if (!ret)
    return NULL;

  g_variant_get (ret, "(@a{sa{sv}})", &inner);

  return g_steal_pointer (&inner);
}

XdpSettings *
_xdp_settings_new (XdpPortal *portal)
{
  XdpSettings *settings;

  settings = g_object_new (XDP_TYPE_SETTINGS, NULL);
  settings->portal = g_object_ref (portal);

  settings->signal_id = g_dbus_connection_signal_subscribe (portal->bus,
                                                            PORTAL_BUS_NAME,
                                                            SETTINGS_INTERFACE,
                                                            "SettingChanged",
                                                            PORTAL_OBJECT_PATH,
                                                            NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                                            settings_changed,
                                                            settings,
                                                            NULL);
  return settings;
}
