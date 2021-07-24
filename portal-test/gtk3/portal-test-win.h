#pragma once

#include <gtk/gtk.h>
#include "portal-test-app.h"

G_DECLARE_FINAL_TYPE(PortalTestWin, portal_test_win, PORTAL, TEST_WIN, GtkApplicationWindow)

GType                    portal_test_win_get_type       (void);
GtkApplicationWindow    *portal_test_win_new            (PortalTestApp *app);
void                     portal_test_win_ack            (PortalTestWin *win);
