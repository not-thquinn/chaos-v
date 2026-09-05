#include "mainwindow.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName("ChaosV");
    QCoreApplication::setApplicationName("ChaosV");
    application.setQuitOnLastWindowClosed(true);

    MainWindow window;
    window.show();
    return application.exec();
}
