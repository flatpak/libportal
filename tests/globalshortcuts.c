/* SPDX-License-Identifier: LGPL-3.0-only */

#include "config.h"

#include <libportal/globalshortcuts.h>
#include <libportal/parent-private.h>
#include <libportal/portal-private.h>

#define PORTAL_NAME "org.freedesktop.portal.TestGlobalShortcuts"
#define SHORTCUTS_IFACE "org.freedesktop.portal.GlobalShortcuts"
#define SESSION_PATH "/org/freedesktop/portal/desktop/session/test/globalshortcuts"
#define EXPORTED_HANDLE "wayland:test-parent"

typedef enum {
  NULL_PARENT,
  SYNC_PARENT,
  DEFERRED_PARENT,
  EXPORT_FAILURE,
  CANCEL_BEFORE_EXPORT,
  CANCEL_DURING_EXPORT,
  METHOD_ERROR,
  PORTAL_CANCELLED,
  LEGACY_HANDLE,
  LEGACY_NULL,
} Mode;

typedef struct {
  const char *name;
  gboolean bind;
  Mode mode;
} TestCase;

typedef struct {
  GDBusConnection *bus;
  GDBusNodeInfo *node;
  guint registration;
  GMainLoop *loop;
  XdpPortal *portal;
  XdpGlobalShortcutsSession *session;
  GAsyncResult *result;
  const TestCase *test;
  guint calls;
  guint completions;
  char *parent_handle;
  GVariant *shortcuts;
  GVariant *options;
  guint exports;
  guint unexports;
  XdpParent *pending_parent;
  XdpParentExported pending_callback;
  gpointer pending_data;
} Fixture;

static const char portal_xml[] =
  "<node>"
  "<interface name='org.freedesktop.portal.GlobalShortcuts'>"
  "<method name='CreateSession'>"
  "<arg type='a{sv}' direction='in'/><arg type='o' direction='out'/>"
  "</method>"
  "<method name='BindShortcuts'>"
  "<arg type='o' direction='in'/><arg type='a(sa{sv})' direction='in'/>"
  "<arg type='s' direction='in'/><arg type='a{sv}' direction='in'/>"
  "<arg type='o' direction='out'/>"
  "</method>"
  "<method name='ConfigureShortcuts'>"
  "<arg type='o' direction='in'/><arg type='s' direction='in'/>"
  "<arg type='a{sv}' direction='in'/>"
  "</method>"
  "</interface>"
  "</node>";

static void
return_response (Fixture               *f,
                 const char            *sender,
                 GDBusMethodInvocation *invocation,
                 guint                  response,
                 GVariantBuilder       *results,
                 GVariant              *options)
{
  g_autofree char *sender_path = g_strdup (sender + 1);
  g_autofree char *request_path = NULL;
  g_autoptr(GError) error = NULL;
  const char *token;

  g_assert_true (g_variant_lookup (options, "handle_token", "&s", &token));
  g_strdelimit (sender_path, ".", '_');
  request_path = g_strconcat (REQUEST_PATH_PREFIX, sender_path, "/", token, NULL);

  g_dbus_method_invocation_return_value (invocation, g_variant_new ("(o)", request_path));
  /* Unicast responses also work with libportal's NO_MATCH_RULE subscriptions. */
  g_dbus_connection_emit_signal (f->bus, sender, request_path,
                                  REQUEST_INTERFACE, "Response",
                                  g_variant_new ("(ua{sv})", response, results), &error);
  g_assert_no_error (error);
}

static void
method_call (GDBusConnection       *connection,
             const char            *sender,
             const char            *path,
             const char            *interface,
             const char            *method,
             GVariant              *parameters,
             GDBusMethodInvocation *invocation,
             gpointer               data)
{
  Fixture *f = data;
  GVariantBuilder results;
  const char *session;
  const char *parent;

  g_variant_builder_init (&results, G_VARIANT_TYPE_VARDICT);
  if (g_str_equal (method, "CreateSession"))
    {
      g_autoptr(GVariant) options = NULL;

      g_variant_get (parameters, "(@a{sv})", &options);
      g_variant_builder_add (&results, "{sv}", "session_handle",
                             g_variant_new_string (SESSION_PATH));
      return_response (f, sender, invocation, 0, &results, options);
      return;
    }

  f->calls++;
  if (g_str_equal (method, "BindShortcuts"))
    g_variant_get (parameters, "(&o@a(sa{sv})&s@a{sv})",
                   &session, &f->shortcuts, &parent, &f->options);
  else
    {
      g_assert_cmpstr (method, ==, "ConfigureShortcuts");
      g_variant_get (parameters, "(&o&s@a{sv})", &session, &parent, &f->options);
    }

  g_assert_cmpstr (session, ==, SESSION_PATH);
  f->parent_handle = g_strdup (parent);

  if (f->test->mode == METHOD_ERROR)
    {
      g_variant_builder_clear (&results);
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                 "org.freedesktop.portal.Error.Failed",
                                                 "Test method failure");
      return;
    }

  if (g_str_equal (method, "BindShortcuts"))
    {
      GVariantBuilder shortcuts;
      GVariantBuilder properties;

      g_variant_builder_init (&shortcuts, G_VARIANT_TYPE ("a(sa{sv})"));
      g_variant_builder_init (&properties, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add (&properties, "{sv}", "description", g_variant_new_string ("Do Foo"));
      g_variant_builder_add (&properties, "{sv}", "trigger_description", g_variant_new_string ("Control+F"));
      g_variant_builder_add (&shortcuts, "(sa{sv})", "foo", &properties);
      g_variant_builder_add (&results, "{sv}", "shortcuts", g_variant_builder_end (&shortcuts));
      return_response (f, sender, invocation,
                        f->test->mode == PORTAL_CANCELLED ? 1 : 0, &results, f->options);
    }
  else
    {
      g_variant_builder_clear (&results);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
    }
}

static const GDBusInterfaceVTable vtable = {
  .method_call = method_call,
};

static void
capture_result (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  Fixture *f = data;

  g_assert_null (f->result);
  f->completions++;
  f->result = g_object_ref (result);
  g_main_loop_quit (f->loop);
}

static gboolean
timeout (gpointer data)
{
  g_error ("GlobalShortcuts operation did not complete within 10 seconds");
  return G_SOURCE_REMOVE;
}

static void
wait_result (Fixture *f)
{
  guint timeout_id = g_timeout_add_seconds (10, timeout, NULL);

  g_main_loop_run (f->loop);
  g_source_remove (timeout_id);
  g_assert_nonnull (f->result);
}

static void
fixture_setup (Fixture *f)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  guint result;

  f->loop = g_main_loop_new (NULL, FALSE);
  f->bus = g_dbus_connection_new_for_address_sync (g_getenv ("DBUS_SESSION_BUS_ADDRESS"),
                                                   G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                   G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                   NULL, NULL, &error);
  g_assert_no_error (error);
  reply = g_dbus_connection_call_sync (f->bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus", "RequestName",
                                        g_variant_new ("(su)", PORTAL_NAME, 0),
                                        G_VARIANT_TYPE ("(u)"), G_DBUS_CALL_FLAGS_NONE,
                                        -1, NULL, &error);
  g_assert_no_error (error);
  g_variant_get (reply, "(u)", &result);
  g_assert_cmpuint (result, ==, 1);

  f->node = g_dbus_node_info_new_for_xml (portal_xml, &error);
  g_assert_no_error (error);
  f->registration = g_dbus_connection_register_object (f->bus, PORTAL_OBJECT_PATH,
                                                        f->node->interfaces[0], &vtable,
                                                        f, NULL, &error);
  g_assert_no_error (error);
  f->portal = xdp_portal_initable_new (&error);
  g_assert_no_error (error);
  g_assert_nonnull (f->portal);

  xdp_portal_create_global_shortcuts_session (f->portal, NULL, capture_result, f);
  wait_result (f);
  f->session = xdp_portal_create_global_shortcuts_session_finish (f->portal, f->result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (f->session);
  g_clear_object (&f->result);
  f->completions = 0;
}

static void
fixture_clear (Fixture *f)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;

  g_clear_object (&f->result);
  g_clear_object (&f->session);
  /* Drop the portal's registry references to its test sessions. */
  g_hash_table_remove_all (f->portal->sessions);
  g_clear_object (&f->portal);
  g_dbus_connection_unregister_object (f->bus, f->registration);
  reply = g_dbus_connection_call_sync (f->bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus", "ReleaseName",
                                        g_variant_new ("(s)", PORTAL_NAME),
                                        G_VARIANT_TYPE ("(u)"), G_DBUS_CALL_FLAGS_NONE,
                                        -1, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (reply);
  g_dbus_connection_close_sync (f->bus, NULL, NULL);
  g_clear_object (&f->bus);
  g_clear_pointer (&f->node, g_dbus_node_info_unref);
  g_clear_pointer (&f->loop, g_main_loop_unref);
  g_clear_pointer (&f->parent_handle, g_free);
  g_clear_pointer (&f->shortcuts, g_variant_unref);
  g_clear_pointer (&f->options, g_variant_unref);
  while (g_main_context_iteration (NULL, FALSE))
    ;
}

static gboolean
export_parent (XdpParent         *parent,
               XdpParentExported  callback,
               gpointer           data)
{
  Fixture *f = parent->data;

  f->exports++;
  if (f->test->mode == EXPORT_FAILURE)
    return FALSE;

  if (f->test->mode == DEFERRED_PARENT || f->test->mode == CANCEL_DURING_EXPORT)
    {
      f->pending_parent = parent;
      f->pending_callback = callback;
      f->pending_data = data;
    }
  else
    callback (parent, EXPORTED_HANDLE, data);

  return TRUE;
}

static void
unexport_parent (XdpParent *parent)
{
  Fixture *f = parent->data;

  f->unexports++;
}

static void
test_parent (gconstpointer data)
{
  const TestCase *test = data;
  Fixture f = { .test = test };
  g_autoptr(XdpParent) parent = NULL;
  g_autoptr(GPtrArray) shortcuts = NULL;
  g_autoptr(GPtrArray) assigned = NULL;
  g_autoptr(GCancellable) cancellable = g_cancellable_new ();
  g_autoptr(GError) error = NULL;
  g_autofree char *token = g_strdup ("activation-token-234");
  gpointer parent_object = NULL;
  gboolean legacy = test->mode == LEGACY_HANDLE || test->mode == LEGACY_NULL;
  gboolean has_parent = !legacy && test->mode != NULL_PARENT;
  gboolean cancelled = test->mode == CANCEL_BEFORE_EXPORT || test->mode == CANCEL_DURING_EXPORT ||
                       test->mode == PORTAL_CANCELLED;
  gboolean success;

  fixture_setup (&f);
  shortcuts = g_ptr_array_new_with_free_func ((GDestroyNotify) xdp_global_shortcut_free);
  g_ptr_array_add (shortcuts, xdp_global_shortcut_new ("foo", "Do Foo", "CTRL+F"));
  if (has_parent)
    {
      parent = g_new0 (XdpParent, 1);
      parent->parent_export = export_parent;
      parent->parent_unexport = unexport_parent;
      parent->data = &f;
      parent->object = g_object_new (G_TYPE_OBJECT, NULL);
      parent_object = parent->object;
      g_object_add_weak_pointer (parent->object, &parent_object);
    }

  if (test->mode == CANCEL_BEFORE_EXPORT)
    g_cancellable_cancel (cancellable);

  if (test->bind && legacy)
    xdp_global_shortcuts_session_bind_shortcuts (f.session, shortcuts,
                                                 test->mode == LEGACY_HANDLE ? "x11:234" : NULL,
                                                 cancellable, capture_result, &f);
  else if (test->bind)
    xdp_global_shortcuts_session_bind_shortcuts_with_parent (f.session, shortcuts, parent,
                                                             cancellable, capture_result, &f);
  else if (legacy)
    xdp_global_shortcuts_session_configure_shortcuts (f.session,
                                                      test->mode == LEGACY_HANDLE ? "x11:234" : NULL,
                                                      token, cancellable, capture_result, &f);
  else
    xdp_global_shortcuts_session_configure_shortcuts_with_parent (f.session, parent,
                                                                  token, cancellable, capture_result, &f);

  /* The public API must retain everything needed after the caller frees its inputs. */
  g_clear_pointer (&parent, xdp_parent_free);
  xdp_global_shortcut_set_description (g_ptr_array_index (shortcuts, 0), "Changed after call");
  g_clear_pointer (&shortcuts, g_ptr_array_unref);
  token[0] = 'X';
  g_clear_pointer (&token, g_free);
  g_assert_null (f.result); /* GAsyncReadyCallback must not run inline. */

  if (test->mode == DEFERRED_PARENT || test->mode == CANCEL_DURING_EXPORT)
    {
      g_assert_nonnull (parent_object);
      g_assert_nonnull (f.pending_parent);
      g_assert_cmpuint (f.calls, ==, 0);
      g_assert_cmpuint (f.unexports, ==, 0);
      if (test->mode == CANCEL_DURING_EXPORT)
        g_cancellable_cancel (cancellable);
      f.pending_callback (f.pending_parent, EXPORTED_HANDLE, f.pending_data);
      f.pending_parent = NULL;
    }

  wait_result (&f);
  if (test->bind)
    {
      if (legacy)
        assigned = xdp_global_shortcuts_session_bind_shortcuts_finish (f.session, f.result, &error);
      else
        assigned = xdp_global_shortcuts_session_bind_shortcuts_with_parent_finish (f.session, f.result, &error);
      success = assigned != NULL;
    }
  else if (legacy)
    success = xdp_global_shortcuts_session_configure_shortcuts_finish (f.session, f.result, &error);
  else
    success = xdp_global_shortcuts_session_configure_shortcuts_with_parent_finish (f.session, f.result, &error);

  if (cancelled)
    {
      g_assert_false (success);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    }
  else if (test->mode == EXPORT_FAILURE)
    {
      g_assert_false (success);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
    }
  else if (test->mode == METHOD_ERROR)
    {
      g_assert_false (success);
      g_assert_nonnull (error);
      g_assert_true (g_dbus_error_is_remote_error (error));
    }
  else
    {
      g_assert_no_error (error);
      g_assert_true (success);
      if (test->bind)
        {
          XdpGlobalShortcutAssigned *shortcut;

          g_assert_cmpuint (assigned->len, ==, 1);
          shortcut = g_ptr_array_index (assigned, 0);
          g_assert_cmpstr (xdp_global_shortcut_assigned_get_shortcut_id (shortcut), ==, "foo");
          g_assert_cmpstr (xdp_global_shortcut_assigned_get_trigger_description (shortcut), ==, "Control+F");
        }
    }

  if (test->mode == EXPORT_FAILURE || test->mode == CANCEL_BEFORE_EXPORT || test->mode == CANCEL_DURING_EXPORT)
    g_assert_cmpuint (f.calls, ==, 0);
  else
    {
      g_assert_cmpuint (f.calls, ==, 1);
      g_assert_cmpstr (f.parent_handle, ==, has_parent ? EXPORTED_HANDLE :
                       test->mode == LEGACY_HANDLE ? "x11:234" : "");
      if (test->bind)
        {
          g_autoptr(GVariant) item = g_variant_get_child_value (f.shortcuts, 0);
          g_autoptr(GVariant) properties = g_variant_get_child_value (item, 1);
          const char *description;
          const char *trigger;

          g_assert_true (g_variant_lookup (properties, "description", "&s", &description));
          g_assert_true (g_variant_lookup (properties, "preferred_trigger", "&s", &trigger));
          g_assert_cmpstr (description, ==, "Do Foo");
          g_assert_cmpstr (trigger, ==, "CTRL+F");
        }
      else
        {
          const char *activation_token;

          g_assert_true (g_variant_lookup (f.options, "activation_token", "&s", &activation_token));
          g_assert_cmpstr (activation_token, ==, "activation-token-234");
        }
    }

  g_assert_cmpuint (f.exports, ==, has_parent && test->mode != CANCEL_BEFORE_EXPORT ? 1 : 0);
  g_assert_cmpuint (f.unexports, ==, has_parent && test->mode != EXPORT_FAILURE &&
                   test->mode != CANCEL_BEFORE_EXPORT ? 1 : 0);
  g_assert_null (parent_object);
  g_assert_cmpuint (f.completions, ==, 1);
  fixture_clear (&f);
}

int
main (int argc, char **argv)
{
  static const TestCase tests[] = {
    { "bind/null-parent", TRUE, NULL_PARENT },
    { "bind/synchronous-export", TRUE, SYNC_PARENT },
    { "bind/deferred-export", TRUE, DEFERRED_PARENT },
    { "bind/export-failure", TRUE, EXPORT_FAILURE },
    { "bind/cancel-before-export", TRUE, CANCEL_BEFORE_EXPORT },
    { "bind/cancel-during-export", TRUE, CANCEL_DURING_EXPORT },
    { "bind/method-error", TRUE, METHOD_ERROR },
    { "bind/portal-cancelled", TRUE, PORTAL_CANCELLED },
    { "bind/legacy-handle", TRUE, LEGACY_HANDLE },
    { "bind/legacy-null", TRUE, LEGACY_NULL },
    { "configure/null-parent", FALSE, NULL_PARENT },
    { "configure/synchronous-export", FALSE, SYNC_PARENT },
    { "configure/deferred-export", FALSE, DEFERRED_PARENT },
    { "configure/export-failure", FALSE, EXPORT_FAILURE },
    { "configure/cancel-before-export", FALSE, CANCEL_BEFORE_EXPORT },
    { "configure/cancel-during-export", FALSE, CANCEL_DURING_EXPORT },
    { "configure/method-error", FALSE, METHOD_ERROR },
    { "configure/legacy-handle", FALSE, LEGACY_HANDLE },
    { "configure/legacy-null", FALSE, LEGACY_NULL },
  };
  g_autoptr(GTestDBus) bus = NULL;
  int result;

  g_test_init (&argc, &argv, NULL);
  g_setenv ("LIBPORTAL_TEST_SUITE", "1", TRUE);
  g_setenv ("LIBPORTAL_PORTAL_BUS_NAME", PORTAL_NAME, TRUE);
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);
  for (guint i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_autofree char *name = g_strconcat ("/globalshortcuts/", tests[i].name, NULL);

      g_test_add_data_func (name, &tests[i], test_parent);
    }
  result = g_test_run ();
  g_test_dbus_down (bus);
  return result;
}
