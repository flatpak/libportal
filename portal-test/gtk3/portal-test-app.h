#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE(PortalTestApp, portal_test_app, PORTAL, TEST_APP, GtkApplication)

GType            portal_test_app_get_type              (void);
GApplication    *portal_test_app_new                   (void);
void             portal_test_app_restart               (PortalTestApp *app);
void             portal_test_app_stop_running_instance (void);
