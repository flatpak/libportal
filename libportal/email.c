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

#include "portal-private.h"
#include "utils.h"

G_DECLARE_FINAL_TYPE (EmailCall, email_call, EMAIL, CALL, GObject)

struct _EmailCall {
  GObject parent_instance;

  XdpPortal *portal;
  GtkWindow *parent;
  gchar *parent_handle;
  char *address;
  char *subject;
  char *body;
  char **attachments;
  GTask *task;
  guint response_signal;
};

struct _EmailCallClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE (EmailCall, email_call, G_TYPE_OBJECT)

static void
email_call_finalize (GObject *object)
{
  EmailCall *call = EMAIL_CALL (object);

  g_free (call->address);
  g_free (call->subject);
  g_free (call->body);
  g_strfreev (call->attachments);

  if (call->response_signal)
    {
      GDBusConnection *bus;

      bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (call->portal->email));
      g_dbus_connection_signal_unsubscribe (bus, call->response_signal);
    }
  g_object_unref (call->portal);
  if (call->parent)
    {
      _gtk_window_unexport_handle (call->parent);
      g_object_unref (call->parent);
    }
  if (call->parent_handle)
    g_free (call->parent_handle);
  g_object_unref (call->task);

  G_OBJECT_CLASS (email_call_parent_class)->finalize (object);
}

static void
email_call_class_init (EmailCallClass *class)
{
  G_OBJECT_CLASS (class)->finalize = email_call_finalize;
}

static void
email_call_init (EmailCall *call)
{
}

static EmailCall *
email_call_new (XdpPortal *portal,
                GtkWindow *parent,
                const char *address,
                const char *subject,
                const char *body,
                const char *const *attachments,
                GTask *task)
{
  EmailCall *call = g_object_new (email_call_get_type (), NULL);

  call->portal = g_object_ref (portal);
  call->parent = parent ? g_object_ref (parent) : NULL;
  call->parent_handle = NULL;
  call->address = g_strdup (address);
  call->subject = g_strdup (subject);
  call->body = g_strdup (body);
  call->attachments = g_strdupv ((char **)attachments);
  call->task = g_object_ref (task);

  return call;
}

static void do_email (EmailCall *call);

static void
got_proxy (GObject *source,
           GAsyncResult *res,
           gpointer data)
{
  g_autoptr(EmailCall) call = data;
  g_autoptr(GError) error = NULL;

  call->portal->email = _xdp_email_proxy_new_for_bus_finish (res, &error);
  if (call->portal->email == NULL)
    {
      g_task_return_error (call->task, error);
      return;
    }

  do_email (call);
}

static void
window_handle_exported (GtkWindow *window,
                        const char *window_handle,
                        gpointer data)
{
  g_autoptr(EmailCall) call = data;

  call->parent_handle = g_strdup (window_handle);

  do_email (call);
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
  g_autoptr(EmailCall) call = data;
  guint32 response;
  g_autoptr(GVariant) ret = NULL;

  g_dbus_connection_signal_unsubscribe (bus, call->response_signal);
  call->response_signal = 0;

  g_variant_get (parameters, "(u@a{sv})", &response, &ret);

g_print ("email response: %d\n", response);
  if (response == 0)
    g_task_return_boolean (call->task, TRUE);
  else if (response == 1)
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Email canceled");
  else
    g_task_return_new_error (call->task, G_IO_ERROR, G_IO_ERROR_FAILED, "Email failed");
}

static void
do_email (EmailCall *call)
{
  GVariantBuilder options;
  GDBusConnection *bus;
  g_autofree char *token = NULL;
  g_autofree char *sender = NULL;
  g_autofree char *handle = NULL;
  int i;

  if (call->portal->email == NULL)
    {
       _xdp_email_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     PORTAL_BUS_NAME,
                                     PORTAL_OBJECT_PATH,
                                     g_task_get_cancellable (call->task),
                                     got_proxy,
                                     g_object_ref (call));
       return;
    }

  if (call->parent_handle == NULL)
    {
      if (call->parent != NULL)
        {
          _gtk_window_export_handle (call->parent,
                                     window_handle_exported,
                                     g_object_ref (call));
          return;
        }

      call->parent_handle = g_strdup ("");
    }

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (call->portal->email));

  token = g_strdup_printf ("portal%d", g_random_int_range (0, G_MAXINT));
  sender = g_strdup (g_dbus_connection_get_unique_name (bus) + 1);
  for (i = 0; sender[i]; i++)
    if (sender[i] == '.')
      sender[i] = '_';

  handle = g_strdup_printf ("/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
  call->response_signal = g_dbus_connection_signal_subscribe (bus,
                                                              PORTAL_BUS_NAME,
                                                              REQUEST_INTERFACE,
                                                              "Response",
                                                              handle,
                                                              NULL,
                                                              G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                              response_received,
                                                              g_object_ref (call),
                                                              NULL);

  g_variant_builder_init (&options, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&options, "{sv}", "handle_token", g_variant_new_string (token));
  if (call->address)
    g_variant_builder_add (&options, "{sv}", "address", g_variant_new_string (call->address));
  if (call->subject)
    g_variant_builder_add (&options, "{sv}", "subject", g_variant_new_string (call->subject));
  if (call->body)
    g_variant_builder_add (&options, "{sv}", "body", g_variant_new_string (call->body));
  /* FIXME: attachments */

  _xdp_email_call_compose_email (call->portal->email,
                                 call->parent_handle,
                                 g_variant_builder_end (&options),
                                 g_task_get_cancellable (call->task),
                                 NULL,
                                 NULL);
}

void
xdp_portal_compose_email (XdpPortal *portal,
                          GtkWindow *parent,
                          const char *address,
                          const char *subject,
                          const char *body,
                          const char *const *attachments,
                          GCancellable *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer callback_data)
{
  g_autoptr(EmailCall) call = NULL;
  g_autoptr(GTask) task = NULL;

g_print ("compose mail\n");
  task = g_task_new (portal, cancellable, callback, callback_data);
  call = email_call_new (portal, parent, address, subject, body, attachments, task);

  do_email (call);
}

gboolean
xdp_portal_compose_email_finish (XdpPortal *portal,
                                 GAsyncResult *result,
                                 GError **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}
