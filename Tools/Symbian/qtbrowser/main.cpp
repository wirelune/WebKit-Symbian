// The Qt browser: a Qt Quick Components front end on the Symbian port's
// WebKit, which runs in wkengine.exe (installed by browser.sis).
#include "pagestore.h"
#include "webpageitem.h"

#include <QApplication>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QDeclarativeView>
#include <QtDeclarative>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qmlRegisterType<WebPageItem>("WebKitSymbian", 1, 0, "WebPage");

    PageStore pages;
    QDeclarativeView view;
    view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
    view.rootContext()->setContextProperty(QLatin1String("pageStore"), &pages);
    // A URL on the command line opens first.
    QStringList arguments = app.arguments();
    view.rootContext()->setContextProperty(QLatin1String("startUrl"),
        arguments.count() > 1 ? arguments.at(1) : QString());
    QObject::connect(view.engine(), SIGNAL(quit()), &app, SLOT(quit()));
    view.setSource(QUrl(QLatin1String("qrc:/qml/main.qml")));
    view.showFullScreen();
    return app.exec();
}
