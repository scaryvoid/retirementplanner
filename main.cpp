#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Retirement Planner");

    MainWindow w;
    w.resize(1240, 780);
    w.show();

    return app.exec();
}
