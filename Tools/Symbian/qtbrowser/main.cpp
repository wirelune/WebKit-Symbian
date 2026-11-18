// The Qt browser: a Qt Quick Components front end on the Symbian port's
// WebKit, which runs in wkengine.exe (installed by browser.sis).
#include "pagestore.h"
#include "webpageitem.h"

#include <QApplication>
#include <QDateTime>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QDeclarativeError>
#include <QDeclarativeView>
#include <QDir>
#include <QtDeclarative>
#include <stdio.h>

namespace {

FILE *logFile = 0;

// Qt's messages, QML errors among them, go to C:\Data\WebKitQt\qt.txt
// (qt.old.txt keeps the previous run's).
void messageHandler(QtMsgType type, const char *message)
{
    static const char *const kinds[] = { "debug", "warning", "critical", "fatal" };
    const char *kind = type <= QtFatalMsg ? kinds[type] : "?";
    QByteArray time = QTime::currentTime().toString(QLatin1String("hh:mm:ss.zzz")).toLatin1();
    if (logFile) {
        fprintf(logFile, "%s %s: %s\n", time.constData(), kind, message);
        fflush(logFile);
    }
    fprintf(stderr, "%s %s: %s\n", time.constData(), kind, message);
    if (type == QtFatalMsg)
        abort();
}

void openLog()
{
#ifdef Q_OS_SYMBIAN
    const QString directory = QLatin1String("C:/Data/WebKitQt");
#else
    const QString directory = QDir::currentPath();
#endif
    QDir().mkpath(directory);
    QString path = directory + QLatin1String("/qt.txt");
    QString old = directory + QLatin1String("/qt.old.txt");
    QFile::remove(old);
    QFile::rename(path, old);
    logFile = fopen(QFile::encodeName(path).constData(), "w");
    qInstallMsgHandler(messageHandler);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    openLog();
    qDebug("WebKit Qt starting, Qt %s", qVersion());
    qmlRegisterType<WebPageItem>("WebKitSymbian", 1, 0, "WebPage");

    PageStore pages;
    QDeclarativeView view;
    view.setResizeMode(QDeclarativeView::SizeRootObjectToView);
    view.rootContext()->setContextProperty(QLatin1String("pageStore"), &pages);
    // A URL on the command line opens first.
    QStringList arguments = app.arguments();
    view.rootContext()->setContextProperty(QLatin1String("startUrl"),
        arguments.count() > 1 ? arguments.at(1) : QString(QLatin1String("")));
    QObject::connect(view.engine(), SIGNAL(quit()), &app, SLOT(quit()));
    view.setSource(QUrl(QLatin1String("qrc:/qml/main.qml")));
    foreach (const QDeclarativeError &error, view.errors())
        qWarning("QML: %s", qPrintable(error.toString()));
    qDebug("QML status %d", view.status());
    view.showFullScreen();
    return app.exec();
}
