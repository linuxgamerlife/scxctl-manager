#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SCX Scheduler Manager");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("scx-scheduler-manager");

    // Generate a simple icon
    QPixmap iconPm(64, 64);
    iconPm.fill(Qt::transparent);
    QPainter ip(&iconPm);
    ip.setRenderHint(QPainter::Antialiasing);
    ip.setBrush(QColor("#1f6feb"));
    ip.setPen(Qt::NoPen);
    ip.drawEllipse(4, 4, 56, 56);
    ip.setBrush(QColor("#58a6ff"));
    ip.drawEllipse(16, 16, 32, 32);
    ip.end();
    app.setWindowIcon(QIcon(iconPm));

    MainWindow w;
    w.show();
    return app.exec();
}
