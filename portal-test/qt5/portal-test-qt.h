#pragma once

#include <QMainWindow>

#undef signals
#include "libportal/portal-qt5.h"
#define signals Q_SIGNALS

class Ui_PortalTestQt;

class PortalTestQt : public QMainWindow
{
    Q_OBJECT
public:
    PortalTestQt(QApplication *app, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~PortalTestQt();

    void updateLastOpenedFile(const QString &file);
private:
    static void openedFile(GObject *object, GAsyncResult *result, gpointer data);

    Ui_PortalTestQt *m_mainWindow;
    QApplication *m_app;
    XdpPortal *m_portal;
};
