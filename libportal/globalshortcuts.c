/*
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

#include "globalshortcuts.h"
#include "portal-private.h"
#include "session-private.h"

/**
 * XdpGlobalShortcutsSession
 *
 * A representation of a long-lived global shortcuts portal interaction.
 *
 * The [class@GlobalShortcutsSession] object is used to represent portal
 * interactions with the global shortcuts desktop portal that extend over
 * multiple portal calls. Usually a caller creates a global shortcuts session,
 * binds shortcuts, and then starts listening to activations.
 *
 * To list current assignments after [method@GlobalShortcutsSession.bind_shortcuts] returns,
 * call [method@GlobalShortcutsSession.list_shortcuts].
 *
 * The [class@GlobalShortcutsSession] wraps a [class@Session] object.
 */

enum {
  SIGNAL_ACTIVATED,
  SIGNAL_DEACTIVATED,
  SIGNAL_SHORTCUTS_CHANGED,
  SIGNAL_LAST_SIGNAL,
};

static guint signals[SIGNAL_LAST_SIGNAL];

struct _XdpGlobalShortcutsSession
{
  GObject     parent_instance;
  XdpSession *parent_session; /* strong ref */
  guint       signal_ids[SIGNAL_LAST_SIGNAL];
};

G_DEFINE_TYPE (XdpGlobalShortcutsSession, xdp_global_shortcuts_session, G_TYPE_OBJECT)

static gboolean
_xdp_global_shortcuts_session_is_valid (XdpGlobalShortcutsSession *session)
{
  return XDP_IS_GLOBAL_SHORTCUTS_SESSION (session) && session->parent_session != NULL;
}

static void
parent_session_destroy (gpointer data, GObject *old_session)
{
  XdpGlobalShortcutsSession *session = XDP_GLOBAL_SHORTCUTS_SESSION (data);

  g_critical ("XdpSession destroyed before XdpGlobalShortcutsSession, you lost count of your session refs");

  session->parent_session = NULL;
}

static void
xdp_global_shortcuts_session_finalize (GObject *object)
{
  XdpGlobalShortcutsSession *session = XDP_GLOBAL_SHORTCUTS_SESSION (object);
  XdpSession *parent_session = session->parent_session;

  if (parent_session == NULL)
    {
      g_critical ("XdpSession destroyed before XdpGlobalShortcutsSession, you lost count of your session refs");
    }
  else
    {
      for (guint i = 0; i < SIGNAL_LAST_SIGNAL; i++)
        {
          guint signal_id = session->signal_ids[i];
          if (signal_id > 0)
            g_dbus_connection_signal_unsubscribe (parent_session->portal->bus, signal_id);
        }

      g_object_weak_unref (G_OBJECT (parent_session), parent_session_destroy, session);

      g_clear_pointer (&session->parent_session, g_object_unref);
    }

  G_OBJECT_CLASS (xdp_global_shortcuts_session_parent_class)->finalize (object);
}

static void
xdp_global_shortcuts_session_class_init (XdpGlobalShortcutsSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xdp_global_shortcuts_session_finalize;

  /**
   * XdpGlobalShortcutsSession::shortcuts-changed:
   * @session: the [class@GlobalShortcutsSession]
   * @shortcuts: a [struct@GLib.Variant] containing the updated shortcuts.
   *
   * Emitted when a GlobalShortcuts session's shortcuts have changed. This
   * signal is emitted after new shortcuts have already become effective.
   */
  signals[SIGNAL_SHORTCUTS_CHANGED] =
    g_signal_new ("shortcuts-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);
  /**
   * XdpGlobalShortcutsSession::activated:
   * @session: the [class@GlobalShortcutsSession]
   * @shortcut_id: shortcut ID
   * @timestamp: time of activation with millisecond granularity
   * @options: signal options
   *
   * Emitted when a GlobalShortcuts shortcut of this session was activated.
   */
  signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  G_TYPE_UINT64,
                  G_TYPE_VARIANT);
  /**
   * XdpGlobalShortcutsSession::deactivated:
   * @session: the [class@GlobalShortcutsSession]
   * @shortcut_id: shortcut ID
   * @timestamp: time of deactivation with millisecond granularity
   * @options: signal options
   *
   * Emitted when a GlobalShortcuts shortcut of this session was deactivated.
   */
  signals[SIGNAL_DEACTIVATED] =
    g_signal_new ("deactivated",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 3,
                  G_TYPE_STRING,
                  G_TYPE_UINT64,
                  G_TYPE_VARIANT);
}

static void
xdp_global_shortcuts_session_init (XdpGlobalShortcutsSession *session)
{
  session->parent_session = NULL;
  for (guint i = 0; i < SIGNAL_LAST_SIGNAL; i++)
    session->signal_ids[i] = 0;
}

/* A request-based method call */
typedef struct {
  XdpPortal                 *portal;
  char                      *session_path; /* object path for session */
  GTask                     *task;
  guint                      signal_id; /* Request::Response signal */
  char                      *request_path; /* object path for request */
  gulong                     cancelled_id; /* signal id for cancelled gobject signal */
  XdpGlobalShortcutsSession *session;

} Call;

static void create_session (Call *call);

static void
call_disconnect_cancelled (Call *call)
{
  GCancellable *cancellable = NULL;

  if (call->task != NULL)
    cancellable = g_task_get_cancellable (call->task);

  if (call->cancelled_id && cancellable)
    g_signal_handler_disconnect (cancellable, call->cancelled_id);

  call->cancelled_id = 0;
}

static void
call_dispose (void *p)
{
  Call *call = p;

  if (call->signal_id)
    g_dbus_connection_signal_unsubscribe (call->portal->bus, g_steal_handle_id (&call->signal_id));

  call_disconnect_cancelled (call);

  g_clear_pointer (&call->request_path, g_free);

  g_clear_object (&call->portal);
  g_clear_object (&call->session);
  g_clear_pointer (&call->session_path, g_free);
  g_clear_object (&call->task);
}

static inline Call *
call_ref (Call *call)
{
  return g_rc_box_acquire (call);
}

static inline void
call_unref (void *call)
{
  g_rc_box_release_full (call, call_dispose);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Call, call_unref)

static Call *
call_new (XdpPortal                   *portal,
          XdpGlobalShortcutsSession   *session,
          void                        *source_object,
          GCancellable                *cancellable,
          GAsyncReadyCallback          callback,
          void                        *callback_data)
{
  g_autoptr(Call) call = g_rc_box_new0 (Call);

  call->portal = g_object_ref (portal);
  if (session != NULL)
    call->session = g_object_ref (session);

  call->task = g_task_new (G_OBJECT (source_object), cancellable, callback, callback_data);
  g_task_set_task_data (call->task, call_ref (call), call_unref);

  return g_steal_pointer (&call);
}

static void
call_returned (GObject *object,
               GAsyncResult *result,
               gpointer data)
{
  g_autoptr(Call) call = data;
  GError *error = NULL;
  g_autoptr(GVariant) ret = NULL;

  if (call->task == NULL)
    return;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object), result, &error);
  if (error)
    {
      call_disconnect_cancelled (call);
      g_task_return_error (call->task, error);
      call_dispose (call);
    }
}

static gboolean
handle_matches_session (XdpGlobalShortcutsSession *session, const char *id)
{
  const char *sid = session->parent_session->id;

  return g_str_equal (sid, id);
}


static void
prep_call (Call *call, GDBusSignalCallback callback, GVariantBuilder *options, void *userdata)
{
  g_autofree char *token = NULL;

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  call->request_path = g_strconcat (REQUEST_PATH_PREFIX, call->portal->sender, "/", token, NULL);
  call->signal_id = g_dbus_connection_signal_subscribe (call->portal->bus,
                                                        PORTAL_BUS_NAME,
                                                        REQUEST_INTERFACE,
                                                        "Response",
                                                        call->request_path,
                                                        NULL,
                                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                        callback,
                                                        call,
                                                        userdata);

  g_variant_builder_init (options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (options, "{sv}", "handle_token", g_variant_new_string (token));
}

static void
xdp_global_shortcut_assigned_clear (struct XdpGlobalShortcutAssigned *shortcut)
{
  g_clear_pointer (&shortcut->shortcut_id, g_free);
  g_clear_pointer (&shortcut->description, g_free);
  g_clear_pointer (&shortcut->trigger_description, g_free);
}

static GArray *
parse_shortcuts_variant (GVariant *shortcuts)
{
  g_autoptr(GArray) ret = NULL;
  GVariantIter iter;
  const char *shortcut_id;
  GVariant *item;

  ret = g_array_new (FALSE, FALSE, sizeof (struct XdpGlobalShortcutAssigned));
  g_array_set_clear_func (ret, (GDestroyNotify) xdp_global_shortcut_assigned_clear);

  g_variant_iter_init (&iter, shortcuts);
  while (g_variant_iter_next (&iter, "(&s@a{sv})", &shortcut_id, &item))
    {
      g_autoptr(GVariant) item_owned = item;
      struct XdpGlobalShortcutAssigned shortcut = { 0, };

      shortcut.shortcut_id = g_strdup (shortcut_id);

      g_variant_lookup (item_owned, "description", "s", &shortcut.description);
      if (shortcut.description == NULL)
        shortcut.description = g_strdup ("");

      g_variant_lookup (item_owned, "trigger_description", "s", &shortcut.trigger_description);
      if (shortcut.trigger_description == NULL)
        shortcut.trigger_description = g_strdup ("");

      g_array_append_val (ret, shortcut);
    }

  return g_steal_pointer (&ret);
}

static GArray *
parse_shortcuts_response (GVariant *response,
                          GError  **error)
{
  g_autoptr(GVariant) shortcuts = NULL;

  shortcuts = g_variant_lookup_value (response,
                                      "shortcuts",
                                      G_VARIANT_TYPE ("a(sa{sv})"));
  if (shortcuts == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "GlobalShortcuts response did not contain a shortcuts array");
      return NULL;
    }

  return parse_shortcuts_variant (shortcuts);
}

static void
shortcuts_changed (GDBusConnection *bus,
                   const char      *sender_name,
                   const char      *object_path,
                   const char      *interface_name,
                   const char      *signal_name,
                   GVariant        *parameters,
                   gpointer         data)
{
  XdpGlobalShortcutsSession *session = XDP_GLOBAL_SHORTCUTS_SESSION (data);
  const char *handle = NULL;
  g_autoptr(GVariant) shortcuts = NULL;

  g_variant_get (parameters, "(&o@a(sa{sv}))", &handle, &shortcuts);

  if (!handle_matches_session (session, handle))
    return;

  g_signal_emit (session, signals[SIGNAL_SHORTCUTS_CHANGED], 0, shortcuts);
}

static void
activated (GDBusConnection *bus,
           const char      *sender_name,
           const char      *object_path,
           const char      *interface_name,
           const char      *signal_name,
           GVariant        *parameters,
           gpointer         data)
{
  XdpGlobalShortcutsSession *session = XDP_GLOBAL_SHORTCUTS_SESSION (data);
  const char *handle = NULL;
  const char *shortcut_id = NULL;
  guint64 timestamp = 0;
  g_autoptr(GVariant) options = NULL;

  g_variant_get (parameters, "(&o&st@a{sv})",
                 &handle,
                 &shortcut_id,
                 &timestamp,
                 &options);

  if (!handle_matches_session (session, handle))
    return;

  g_signal_emit (session, signals[SIGNAL_ACTIVATED], 0, shortcut_id, timestamp, options);
}

static void
deactivated (GDBusConnection *bus,
             const char      *sender_name,
             const char      *object_path,
             const char      *interface_name,
             const char      *signal_name,
             GVariant        *parameters,
             gpointer         data)
{
  XdpGlobalShortcutsSession *session = XDP_GLOBAL_SHORTCUTS_SESSION (data);
  const char *handle = NULL;
  const char *shortcut_id = NULL;
  guint64 timestamp = 0;
  g_autoptr(GVariant) options = NULL;

  g_variant_get (parameters, "(&o&st@a{sv})",
                 &handle,
                 &shortcut_id,
                 &timestamp,
                 &options);

  if (!handle_matches_session (session, handle))
    return;

  g_signal_emit (session, signals[SIGNAL_DEACTIVATED], 0, shortcut_id, timestamp, options);
}

static XdpGlobalShortcutsSession *
_xdp_global_shortcuts_session_new (XdpPortal *portal, const char *session_path)
{
  XdpSession *parent_session;
  XdpGlobalShortcutsSession *session;

  parent_session = _xdp_session_new (portal, session_path, XDP_SESSION_GLOBAL_SHORTCUTS);
  session = g_object_new (XDP_TYPE_GLOBAL_SHORTCUTS_SESSION, NULL);

  g_object_weak_ref (G_OBJECT (parent_session), parent_session_destroy, session);
  session->parent_session = parent_session;

  session->signal_ids[SIGNAL_SHORTCUTS_CHANGED] =
    g_dbus_connection_signal_subscribe (portal->bus,
                                        PORTAL_BUS_NAME,
                                        "org.freedesktop.portal.GlobalShortcuts",
                                        "ShortcutsChanged",
                                        PORTAL_OBJECT_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        shortcuts_changed,
                                        session,
                                        NULL);

  session->signal_ids[SIGNAL_ACTIVATED] =
    g_dbus_connection_signal_subscribe (portal->bus,
                                        PORTAL_BUS_NAME,
                                        "org.freedesktop.portal.GlobalShortcuts",
                                        "Activated",
                                        PORTAL_OBJECT_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        activated,
                                        session,
                                        NULL);

  session->signal_ids[SIGNAL_DEACTIVATED] =
    g_dbus_connection_signal_subscribe (portal->bus,
                                        PORTAL_BUS_NAME,
                                        "org.freedesktop.portal.GlobalShortcuts",
                                        "Deactivated",
                                        PORTAL_OBJECT_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                        deactivated,
                                        session,
                                        NULL);

  return session;
}

void
xdp_global_shortcuts_session_close (XdpGlobalShortcutsSession *session)
{
  g_return_if_fail (_xdp_global_shortcuts_session_is_valid (session));

  xdp_session_close (session->parent_session);
}

static void
bind_shortcuts_done (GDBusConnection *bus,
                     const char      *sender_name,
                     const char      *object_path,
                     const char      *interface_name,
                     const char      *signal_name,
                     GVariant        *parameters,
                     gpointer         data)
{
  Call *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response != 0)
    call_disconnect_cancelled (call);

  if (response == 0)
    {
      g_autoptr(GArray) shortcuts = NULL;

      g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);
      call->signal_id = 0;

      shortcuts = parse_shortcuts_response (ret, &error);
      if (shortcuts != NULL)
        g_task_return_pointer (call->task,
                               g_steal_pointer (&shortcuts),
                               (GDestroyNotify) g_array_unref);
      else
        g_task_return_error (call->task, g_steal_pointer (&error));
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "GlobalShortcuts BindShortcuts() canceled");
  else if (response == 2)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "GlobalShortcuts BindShortcuts() failed");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "GlobalShortcuts BindShortcuts() failed with response %u", response);

  call_dispose (call);
}

static void
session_created (GDBusConnection *bus,
                 const char      *sender_name,
                 const char      *object_path,
                 const char      *interface_name,
                 const char      *signal_name,
                 GVariant        *parameters,
                 gpointer         data)
{
  Call *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response != 0)
    call_disconnect_cancelled (call);

  if (response == 0)
    {
      g_dbus_connection_signal_unsubscribe (call->portal->bus, call->signal_id);
      call->signal_id = 0;

      if (!g_variant_lookup (ret, "session_handle", "s", &call->session_path))
        g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "CreateSession failed to return a session handle");
      else
        {
          call->session = _xdp_global_shortcuts_session_new (call->portal, call->session_path);
          g_task_return_pointer (call->task,
                                 g_steal_pointer (&call->session),
                                 g_object_unref);
        }
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "CreateSession canceled");
  else if (response == 2)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "CreateSession failed");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "CreateSession failed with response %u", response);

  call_dispose (call);
}

static void
call_cancelled_cb (GCancellable *cancellable,
                   gpointer     data)
{
  Call *call = data;

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

static void
create_session (Call *call)
{
  GVariantBuilder options;
  g_autofree char *session_token = NULL;
  GCancellable *cancellable;

  session_token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));

  prep_call (call, session_created, &options, NULL);
  g_variant_builder_add (&options, "{sv}", "session_handle_token", g_variant_new_string (session_token));
  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (call_cancelled_cb), call);

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.GlobalShortcuts",
                          "CreateSession",
                          g_variant_new ("(a{sv})", &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          call_returned,
                          call_ref (call));
}

/**
 * xdp_portal_create_global_shortcuts_session:
 * @portal: a [class@Portal]
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: data to pass to @callback
 *
 * Creates a session for global shortcuts
 *
 * When the request is done, @callback will be called. You can then
 * call [method@Portal.create_global_shortcuts_session_finish] to get the results.
 */
void
xdp_portal_create_global_shortcuts_session (XdpPortal           *portal,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             data)
{
  g_autoptr(Call) call = NULL;

  g_return_if_fail (XDP_IS_PORTAL (portal));

  call = call_new (portal, NULL, portal, cancellable, callback, data);
  create_session (call);
}

/**
 * xdp_portal_create_global_shortcuts_session_finish:
 * @portal: a [class@Portal]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the GlobalShortcuts CreateSession request, and returns a
 * [class@GlobalShortcutsSession]. To get to the [class@Session] within use
 * xdp_global_shortcuts_session_get_session().
 *
 * Returns: (transfer full): a [class@GlobalShortcutsSession]
 */
XdpGlobalShortcutsSession *
xdp_portal_create_global_shortcuts_session_finish (XdpPortal    *portal,
                                                   GAsyncResult *result,
                                                   GError      **error)
{
  XdpGlobalShortcutsSession *session;

  g_return_val_if_fail (XDP_IS_PORTAL (portal), NULL);
  g_return_val_if_fail (g_task_is_valid (result, portal), NULL);

  session = g_task_propagate_pointer (G_TASK (result), error);

  if (session)
    return session;
  else
    return NULL;
}

/**
 * xdp_global_shortcuts_session_get_session:
 * @session: a [class@XdpGlobalShortcutsSession]
 *
 * Return the [class@XdpSession] for this GlobalShortcuts session.
 *
 * Returns: (transfer none): a [class@Session] object
 */
XdpSession *
xdp_global_shortcuts_session_get_session (XdpGlobalShortcutsSession *session)
{
  g_return_val_if_fail (_xdp_global_shortcuts_session_is_valid (session), NULL);
  return session->parent_session;
}

static void
list_shortcuts_done (GDBusConnection *bus,
                     const char      *sender_name,
                     const char      *object_path,
                     const char      *interface_name,
                     const char      *signal_name,
                     GVariant        *parameters,
                     gpointer         data)
{
  Call *call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

  if (response == 0)
    {
      g_autoptr(GArray) shortcuts = NULL;

      shortcuts = parse_shortcuts_response (ret, &error);
      if (shortcuts != NULL)
        g_task_return_pointer (call->task,
                               g_steal_pointer (&shortcuts),
                               (GDestroyNotify) g_array_unref);
      else
        g_task_return_error (call->task, g_steal_pointer (&error));
    }
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "GlobalShortcuts ListShortcuts() canceled");
  else if (response == 2)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "GlobalShortcuts ListShortcuts() failed");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "GlobalShortcuts ListShortcuts() failed with response %u", response);

  call_dispose (call);
}

static void
list_shortcuts (Call *call)
{
  GVariantBuilder options;
  GCancellable *cancellable;

  prep_call (call, list_shortcuts_done, &options, NULL);

  cancellable = g_task_get_cancellable (call->task);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (call_cancelled_cb), call);

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.GlobalShortcuts",
                          "ListShortcuts",
                          g_variant_new ("(oa{sv})",
                                         call->session->parent_session->id,
                                         &options),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          call_returned,
                          call_ref (call));
}

/**
 * xdp_global_shortcuts_session_bind_shortcuts:
 * @session: a [class@GlobalShortcutsSession]
 * @shortcuts: (element-type XdpGlobalShortcut):
 *   array of shortcuts to bind
 * @parent_window: (nullable): parent window identifier, or %NULL
 * @cancellable: (nullable): optional [class@Gio.Cancellable]
 * @callback: (scope async): a callback to call when the request is done
 * @data: data to pass to @callback
 *
 * Bind shortcuts and list triggers.
 */
void
xdp_global_shortcuts_session_bind_shortcuts (XdpGlobalShortcutsSession *session,
                                             GArray                    *shortcuts,
                                             const char                *parent_window,
                                             GCancellable              *cancellable,
                                             GAsyncReadyCallback        callback,
                                             gpointer                   data)
{
  g_autoptr(Call) call = NULL;
  XdpPortal *portal;
  guint i;
  GVariantBuilder options;
  GVariantBuilder shortcuts_builder;
  GVariant *v;

  g_return_if_fail (_xdp_global_shortcuts_session_is_valid (session));
  g_return_if_fail (shortcuts != NULL);

  for (i = 0; i < shortcuts->len; i++)
    {
      struct XdpGlobalShortcut *shortcut;

      shortcut = &g_array_index (shortcuts, struct XdpGlobalShortcut, i);

      g_return_if_fail (shortcut->shortcut_id != NULL);
      g_return_if_fail (shortcut->description != NULL);
    }

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);

  portal = session->parent_session->portal;

  call = call_new (portal, session, session, cancellable, callback, data);

  prep_call (call, bind_shortcuts_done, &options, NULL);
  if (cancellable)
    call->cancelled_id = g_signal_connect (cancellable, "cancelled", G_CALLBACK (call_cancelled_cb), call);

  g_variant_builder_init (&shortcuts_builder, G_VARIANT_TYPE ("a(sa{sv})"));

  for (i = 0; i < shortcuts->len; i++)
    {
      GVariantDict dict;
      struct XdpGlobalShortcut *shortcut;

      shortcut = &g_array_index (shortcuts, struct XdpGlobalShortcut, i);
      g_variant_dict_init (&dict, NULL);
      if (shortcut->preferred_trigger)
        g_variant_dict_insert (&dict, "preferred_trigger", "s", shortcut->preferred_trigger);
      g_variant_dict_insert (&dict, "description", "s", shortcut->description);

      g_variant_builder_add (&shortcuts_builder, "(s@a{sv})", shortcut->shortcut_id, g_variant_dict_end (&dict));
    }

  if (parent_window == NULL)
    parent_window = "";

  v = g_variant_new ("(o@a(sa{sv})s@a{sv})",
                     session->parent_session->id,
                     g_variant_builder_end (&shortcuts_builder),
                     parent_window,
                     g_variant_builder_end (&options));

  g_dbus_connection_call (call->portal->bus,
                          PORTAL_BUS_NAME,
                          PORTAL_OBJECT_PATH,
                          "org.freedesktop.portal.GlobalShortcuts",
                          "BindShortcuts",
                          v,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          g_task_get_cancellable (call->task),
                          call_returned,
                          call_ref (call));
}


/**
 * xdp_global_shortcuts_session_bind_shortcuts_finish:
 * @session: a [class@GlobalShortcutsSession]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the GlobalShortcuts BindShortcuts request.
 *
 * Returns: (transfer full) (element-type XdpGlobalShortcutAssigned):
 *   array of assigned shortcuts.
 */
GArray *
xdp_global_shortcuts_session_bind_shortcuts_finish (XdpGlobalShortcutsSession *session,
                                                    GAsyncResult              *result,
                                                    GError                   **error)
{
  g_return_val_if_fail (XDP_IS_GLOBAL_SHORTCUTS_SESSION (session), NULL);
  g_return_val_if_fail (g_task_is_valid (result, session), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * xdp_global_shortcuts_session_list_shortcuts:
 * @session: a [class@GlobalShortcutsSession]
 *
 * List currently registered shortcuts and triggers.
 */
void
xdp_global_shortcuts_session_list_shortcuts (XdpGlobalShortcutsSession *session,
                                             GCancellable              *cancellable,
                                             GAsyncReadyCallback        callback,
                                             gpointer                  data)
{
  g_autoptr(Call) call = NULL;
  XdpPortal *portal;

  g_return_if_fail (_xdp_global_shortcuts_session_is_valid (session));

  portal = session->parent_session->portal;

  call = call_new (portal, session, session, cancellable, callback, data);
  list_shortcuts (call);
}

/**
 * xdp_global_shortcuts_session_list_shortcuts_finish:
 * @session: a [class@GlobalShortcutsSession]
 * @result: a [iface@Gio.AsyncResult]
 * @error: return location for an error
 *
 * Finishes the list-shortcuts request, and returns a GArray
 * with the shortcuts.
 *
 * Returns: (transfer full) (element-type XdpGlobalShortcutAssigned):
 *   array of assigned shortcuts.
 */

GArray *
xdp_global_shortcuts_session_list_shortcuts_finish (XdpGlobalShortcutsSession *session,
                                                    GAsyncResult              *result,
                                                    GError                   **error)
{
  g_return_val_if_fail (_xdp_global_shortcuts_session_is_valid (session), NULL);
  g_return_val_if_fail (g_task_is_valid (result, session), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
