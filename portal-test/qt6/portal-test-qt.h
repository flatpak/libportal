#pragma once

#include <QMainWindow>

#undef signals
#include "libportal/portal-qt6.h"
#define signals Q_SIGNALS

class Ui_PortalTestQt;

class PortalTestQt : public QMainWindow
{
    Q_OBJECT
public:
    PortalTestQt(XdpPortal *portal, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~PortalTestQt();

    void updateLastOpenedFile(const QString &file);
private:
    static void openedFile(GObject *object, GAsyncResult *result, gpointer data);

    Ui_PortalTestQt *m_mainWindow;
    XdpPortal *m_portal;
};
