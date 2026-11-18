# Qt-браузер для Symbian Anna на Qt 4.7.4 + Qt Quick Components 1.1.
# Страницы рисует движок WebKit 2.32 из этого порта, отдельный процесс
# wkengine.exe (ставится вместе с browser.sis, см. ../engine). Здесь только
# интерфейс; в Qt Simulator движка нет, видна одна оболочка.
TEMPLATE = app
TARGET   = WebKitQt
VERSION  = 0.1.0

QT += core gui declarative
CONFIG += qt-components

HEADERS += engineclient.h webpageitem.h pagestore.h ../engine/EngineProtocol.h
SOURCES += main.cpp webpageitem.cpp pagestore.cpp
RESOURCES += qml.qrc
OTHER_FILES += qml/main.qml qml/BrowserPage.qml qml/PageListPage.qml

symbian {
    SOURCES += engineclient_symbian.cpp
    LIBS += -leuser -lsysutil

    TARGET.UID3 = 0xE1A5C004
    TARGET.CAPABILITY +=
    TARGET.EPOCSTACKSIZE = 0x14000
    TARGET.EPOCHEAPSIZE  = 0x020000 0x2000000

    DEPLOYMENT.display_name = "WebKit Qt"

    vendorinfo = "%{\"symbian-webkit\"}" ":\"symbian-webkit\""
    my_deployment.pkg_prerules += vendorinfo

    # Только Symbian^3 / Anna / Belle
    default_deployment.pkg_prerules -= pkg_platform_dependencies
    my_deployment.pkg_prerules += "[0x20022E6D],0,0,0,{\"S60ProductID\"}"
    # Движок: browser.sis (0xE1A5C012) с wkengine.exe
    my_deployment.pkg_prerules += "(0xE1A5C012),0,3,0,{\"WebKit Browser\"}"

    DEPLOYMENT += my_deployment
} else {
    SOURCES += engineclient_stub.cpp
}
