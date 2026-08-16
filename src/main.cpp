#include <QApplication>
#include <QStyleFactory>

#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SmartAI"));
    QCoreApplication::setApplicationName(QStringLiteral("SmartAI"));
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    MainWindow window;
    window.show();
    return app.exec();
}
