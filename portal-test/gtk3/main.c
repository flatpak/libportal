#include <gtk/gtk.h>
#include <gst/gst.h>

#include "portal-test-app.h"

int
main (int argc, char *argv[])
{
  gst_init (&argc, &argv);

  g_message ("Starting org.gnome.PortalTest.Gtk3");

  if (g_strv_contains ((const char * const *)argv, "--replace"))
    portal_test_app_stop_running_instance ();

  return g_application_run (portal_test_app_new (), argc, argv);
}
