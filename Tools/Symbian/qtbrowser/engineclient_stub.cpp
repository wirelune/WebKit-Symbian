// No engine outside Symbian: the Qt Simulator shows the interface only.
#include "engineclient.h"

#include <QTimer>

class EngineClientPrivate
{
};

EngineClient::State::State()
    : loading(false)
    , progress(0)
    , canGoBack(false)
    , canGoForward(false)
    , inputType(InputNone)
    , textLength(0)
    , maxTextLength(0)
    , selectionAnchor(0)
    , selectionFocus(0)
    , networkError(0)
{
}

EngineClient::EngineClient(QObject *parent)
    : QObject(parent)
    , d(0)
{
}

EngineClient::~EngineClient()
{
}

void EngineClient::start(const QSize &, int)
{
    // KErrNotSupported, after the caller has connected its slots.
    QMetaObject::invokeMethod(this, "failed", Qt::QueuedConnection, Q_ARG(int, -5));
}

bool EngineClient::isReady() const { return false; }
QImage EngineClient::image() const { return QImage(); }
void EngineClient::setSize(const QSize &) { }
void EngineClient::load(const QString &) { }
void EngineClient::reload() { }
void EngineClient::stop() { }
void EngineClient::goBack() { }
void EngineClient::goForward() { }
void EngineClient::setFocused(bool) { }
void EngineClient::pointer(PointerType, const QPoint &) { }
void EngineClient::scrollBy(int, int) { }
void EngineClient::key(int, int, const QString &) { }
void EngineClient::setSelection(int, int) { }
void EngineClient::setComposition(const QString &, int) { }
void EngineClient::confirmComposition(const QString &) { }
void EngineClient::cancelComposition() { }
EngineClient::State EngineClient::state() const { return State(); }
QString EngineClient::title() const { return QString(); }
QString EngineClient::url() const { return QString(); }
QString EngineClient::editableText(int, int) const { return QString(); }
EngineClient::PopupType EngineClient::popupType() const { return PopupNone; }
QString EngineClient::popupMessage() const { return QString(); }
QStringList EngineClient::popupItems(int *) const { return QStringList(); }
void EngineClient::answerPopup(int) { }
