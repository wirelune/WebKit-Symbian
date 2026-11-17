#include "webpageitem.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QGraphicsSceneMouseEvent>
#include <QInputContext>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QWidget>

namespace {

// Windows virtual key codes, which WebCore's key events use.
enum {
    VK_BACK = 0x08, VK_TAB = 0x09, VK_RETURN = 0x0D, VK_ESCAPE = 0x1B, VK_SPACE = 0x20,
    VK_PRIOR = 0x21, VK_NEXT = 0x22, VK_END = 0x23, VK_HOME = 0x24,
    VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28, VK_DELETE = 0x2E
};

// A drag of this many pixels is a scroll, not a tap.
const int KTapSlop = 8;
// The most of a field's text the input method is shown.
const int KMaxSurroundingText = 8192;

int windowsKeyCode(int key)
{
    switch (key) {
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Select: return VK_RETURN;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_End: return VK_END;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Delete: return VK_DELETE;
    }
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9))
        return key;
    return 0;
}

} // namespace

WebPageItem::WebPageItem(QDeclarativeItem *parent)
    : QDeclarativeItem(parent)
    , m_client(new EngineClient(this))
    , m_status(QLatin1String("starting"))
    , m_error(0)
    , m_resizeTimer(new QTimer(this))
    , m_tapped(false)
    , m_editable(false)
    , m_composing(false)
    , m_compositionStart(0)
{
    setFlag(QGraphicsItem::ItemHasNoContents, false);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setClip(true);

    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->setInterval(50);
    connect(m_resizeTimer, SIGNAL(timeout()), SLOT(sendSize()));

    connectClient();

    // Once QML has set the geometry.
    QTimer::singleShot(0, this, SLOT(startEngine()));
}

WebPageItem::~WebPageItem()
{
}

void WebPageItem::connectClient()
{
    connect(m_client, SIGNAL(ready()), SLOT(onReady()));
    connect(m_client, SIGNAL(failed(int)), SLOT(onFailed(int)));
    connect(m_client, SIGNAL(died(int)), SLOT(onDied(int)));
    connect(m_client, SIGNAL(painted(QRect)), SLOT(onPainted(QRect)));
    connect(m_client, SIGNAL(titleChanged()), SLOT(onTitleChanged()));
    connect(m_client, SIGNAL(urlChanged()), SLOT(onUrlChanged()));
    connect(m_client, SIGNAL(progressChanged()), SLOT(onProgressChanged()));
    connect(m_client, SIGNAL(editingChanged()), SLOT(onEditingChanged()));
    connect(m_client, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()));
    connect(m_client, SIGNAL(popupRequested()), SLOT(onPopupRequested()));
    connect(m_client, SIGNAL(networkFailed(int)), SIGNAL(networkFailed(int)));
}

QSize WebPageItem::viewSize() const
{
    return QSize(qRound(width()), qRound(height()));
}

void WebPageItem::startEngine()
{
    QRect screen = QApplication::desktop()->screenGeometry();
    QSize size = viewSize();
    if (size.isEmpty())
        size = screen.size();
    setStatus(QLatin1String("starting"), 0);
    m_client->start(size, qMax(screen.width(), screen.height()));
}

void WebPageItem::restart()
{
    if (m_status == QLatin1String("starting") || m_status == QLatin1String("ready"))
        return;
    m_client->disconnect(this);
    m_client->deleteLater();
    m_client = new EngineClient(this);
    connectClient();
    m_state = EngineClient::State();
    m_editable = false;
    m_composing = false;
    updateInputMethod();
    startEngine();
}

void WebPageItem::setStatus(const QString &status, int error)
{
    if (m_status == status && m_error == error)
        return;
    m_status = status;
    m_error = error;
    emit statusChanged();
}

void WebPageItem::onReady()
{
    setStatus(QLatin1String("ready"), 0);
    sendSize();
    m_client->setFocused(hasFocus());
    QString url = m_url.isEmpty() ? m_initialUrl : m_url;
    if (!url.isEmpty())
        m_client->load(url);
    update();
}

void WebPageItem::onFailed(int error)
{
    setStatus(QLatin1String("failed"), error);
    update();
}

void WebPageItem::onDied(int reason)
{
    setStatus(QLatin1String("died"), reason);
    m_state.loading = false;
    emit progressChanged();
    update();
}

void WebPageItem::sendSize()
{
    if (m_client->isReady())
        m_client->setSize(viewSize());
}

void WebPageItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QDeclarativeItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        m_resizeTimer->start();
}

void WebPageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    QRectF exposed = option->exposedRect.intersected(boundingRect());
    QImage image = m_client->image();
    QRectF page(0, 0, image.width(), image.height());
    QRectF shown = exposed.intersected(page);
    if (!shown.isEmpty())
        painter->drawImage(shown, image, shown);
    // Around the page while a new size is on its way to the engine.
    if (shown != exposed) {
        QRegion rest = QRegion(exposed.toAlignedRect()).subtracted(QRegion(shown.toAlignedRect()));
        foreach (const QRect &rect, rest.rects())
            painter->fillRect(rect, Qt::white);
    }
}

void WebPageItem::onPainted(const QRect &rect)
{
    update(QRectF(rect));
}

void WebPageItem::onTitleChanged()
{
    m_title = m_client->title();
    emit titleChanged();
}

void WebPageItem::onUrlChanged()
{
    m_url = m_client->url();
    emit urlChanged();
}

void WebPageItem::onProgressChanged()
{
    EngineClient::State state = m_client->state();
    m_state.loading = state.loading;
    m_state.progress = state.progress;
    m_state.canGoBack = state.canGoBack;
    m_state.canGoForward = state.canGoForward;
    emit progressChanged();
}

void WebPageItem::load(const QString &url)
{
    QString text = url.trimmed();
    if (text.isEmpty())
        return;
    if (!m_client->isReady()) {
        m_initialUrl = text;
        return;
    }
    m_client->load(text);
}

void WebPageItem::reload()
{
    m_client->reload();
}

void WebPageItem::stop()
{
    m_client->stop();
}

void WebPageItem::goBack()
{
    m_client->goBack();
}

void WebPageItem::goForward()
{
    m_client->goForward();
}

void WebPageItem::answerPopup(int result)
{
    m_client->answerPopup(result);
}

void WebPageItem::onPopupRequested()
{
    switch (m_client->popupType()) {
    case EngineClient::PopupAlert:
        emit alertRequested(m_client->popupMessage());
        break;
    case EngineClient::PopupConfirm:
        emit confirmRequested(m_client->popupMessage());
        break;
    case EngineClient::PopupSelect: {
        int selected = -1;
        QStringList items = m_client->popupItems(&selected);
        emit selectRequested(items, selected);
        break;
    }
    default:
        break;
    }
}

void WebPageItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();
    if (!hasFocus())
        setFocus(true);
    m_pressPosition = event->pos();
    m_tapped = false;
    emit pressed();
    m_client->pointer(EngineClient::PointerDown, event->pos().toPoint());
}

void WebPageItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // The second tap of a double tap is a tap of its own.
    mousePressEvent(event);
}

void WebPageItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();
    m_client->pointer(EngineClient::PointerMove, event->pos().toPoint());
}

void WebPageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();
    m_client->pointer(EngineClient::PointerUp, event->pos().toPoint());
    QPointF moved = event->pos() - m_pressPosition;
    bool tap = qAbs(moved.x()) <= KTapSlop && qAbs(moved.y()) <= KTapSlop;
    if (!tap)
        return;
    // Tapping a field opens the touch keyboard, as for a text field; a field
    // that gets the focus from this tap does when the engine says so.
    if (m_editable)
        showInputPanel(true);
    else
        m_tapped = true;
}

void WebPageItem::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    // Softkeys and the menu belong to the window.
    if (key == Qt::Key_Context1 || key == Qt::Key_Context2 || key == Qt::Key_Back
        || key == Qt::Key_Menu || key == Qt::Key_Call || key == Qt::Key_Hangup) {
        event->ignore();
        return;
    }
    int keyCode = windowsKeyCode(key);
    QString text = event->text();
    if (keyCode == VK_RETURN)
        text = QLatin1String("\r");
    else if (keyCode == VK_BACK || keyCode == VK_DELETE || keyCode == VK_ESCAPE || keyCode == VK_TAB)
        text.clear();
    if (!keyCode && text.isEmpty()) {
        event->ignore();
        return;
    }
    int modifiers = 0;
    if (event->modifiers() & Qt::ShiftModifier)
        modifiers |= EngineClient::ShiftModifier;
    if (event->modifiers() & Qt::ControlModifier)
        modifiers |= EngineClient::ControlModifier;
    if (event->modifiers() & Qt::AltModifier)
        modifiers |= EngineClient::AltModifier;
    m_client->key(keyCode, modifiers, text);
    event->accept();
}

void WebPageItem::keyReleaseEvent(QKeyEvent *event)
{
    // The press sent both.
    event->accept();
}

void WebPageItem::focusInEvent(QFocusEvent *event)
{
    QDeclarativeItem::focusInEvent(event);
    m_client->setFocused(true);
}

void WebPageItem::focusOutEvent(QFocusEvent *event)
{
    QDeclarativeItem::focusOutEvent(event);
    m_client->setFocused(false);
}

void WebPageItem::onEditingChanged()
{
    EngineClient::State state = m_client->state();
    m_state.inputType = state.inputType;
    bool editable = state.inputType != EngineClient::InputNone;
    if (!editable)
        m_composing = false;
    m_editable = editable;
    updateInputMethod();
    if (m_editable && m_tapped)
        showInputPanel(true);
    m_tapped = false;
}

void WebPageItem::onSelectionChanged()
{
    // The input method reads the field again.
    if (m_editable && !m_composing) {
        if (QInputContext *context = qApp->inputContext())
            context->update();
    }
}

void WebPageItem::updateInputMethod()
{
    Qt::InputMethodHints hints = Qt::ImhNone;
    switch (m_state.inputType) {
    case EngineClient::InputPassword:
        hints = Qt::ImhHiddenText | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase;
        break;
    case EngineClient::InputNumber:
        hints = Qt::ImhFormattedNumbersOnly;
        break;
    case EngineClient::InputTelephone:
        hints = Qt::ImhDialableCharactersOnly;
        break;
    case EngineClient::InputEmail:
        hints = Qt::ImhEmailCharactersOnly | Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;
        break;
    case EngineClient::InputUrl:
        hints = Qt::ImhUrlCharactersOnly | Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText;
        break;
    default:
        break;
    }
    setInputMethodHints(hints);
    setFlag(QGraphicsItem::ItemAcceptsInputMethod, m_editable);
    if (QInputContext *context = qApp->inputContext())
        context->reset();
}

void WebPageItem::showInputPanel(bool show)
{
    QWidget *widget = QApplication::focusWidget();
    if (!widget)
        return;
    QEvent event(show ? QEvent::RequestSoftwareInputPanel : QEvent::CloseSoftwareInputPanel);
    QApplication::sendEvent(widget, &event);
}

QVariant WebPageItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (!m_editable)
        return QDeclarativeItem::inputMethodQuery(query);
    EngineClient::State state = m_client->state();
    int anchor = state.selectionAnchor;
    int cursor = state.selectionFocus;
    int length = state.textLength;
    // The engine's text includes the composition, the input method's
    // doesn't: it adds its inline text itself.
    if (m_composing) {
        length -= m_composition.length();
        anchor = cursor = m_compositionStart;
    }
    switch (query) {
    case Qt::ImMicroFocus:
        return QRectF(state.caret);
    case Qt::ImCursorPosition:
        return cursor;
    case Qt::ImAnchorPosition:
        return anchor;
    case Qt::ImMaximumTextLength:
        return state.maxTextLength > 0 ? QVariant(state.maxTextLength) : QVariant();
    case Qt::ImSurroundingText:
    case Qt::ImCurrentSelection: {
        QString text = m_client->editableText(0, qMin(state.textLength, KMaxSurroundingText));
        if (m_composing)
            text.remove(m_compositionStart, m_composition.length());
        if (query == Qt::ImSurroundingText)
            return text;
        return text.mid(qMin(anchor, cursor), qAbs(cursor - anchor));
    }
    default:
        return QDeclarativeItem::inputMethodQuery(query);
    }
}

void WebPageItem::inputMethodEvent(QInputMethodEvent *event)
{
    event->accept();
    if (!m_editable)
        return;
    if (event->replacementLength()) {
        EngineClient::State state = m_client->state();
        int start = (m_composing ? m_compositionStart : state.selectionFocus) + event->replacementStart();
        if (m_composing) {
            m_client->cancelComposition();
            m_composing = false;
        }
        m_client->setSelection(start, start + event->replacementLength());
    }

    const QString commit = event->commitString();
    const QString preedit = event->preeditString();
    if (!commit.isEmpty()) {
        m_client->confirmComposition(commit);
        m_composing = false;
    } else if (preedit.isEmpty() && m_composing) {
        // The inline text was emptied or cancelled.
        m_client->cancelComposition();
        m_composing = false;
    }

    if (!preedit.isEmpty()) {
        int cursor = preedit.length();
        foreach (const QInputMethodEvent::Attribute &attribute, event->attributes()) {
            if (attribute.type == QInputMethodEvent::Cursor)
                cursor = attribute.start;
        }
        if (!m_composing) {
            EngineClient::State state = m_client->state();
            m_compositionStart = qMin(state.selectionAnchor, state.selectionFocus);
        }
        m_client->setComposition(preedit, cursor);
        m_composition = preedit;
        m_composing = true;
    } else
        m_composition.clear();

    foreach (const QInputMethodEvent::Attribute &attribute, event->attributes()) {
        if (attribute.type == QInputMethodEvent::Selection && !m_composing)
            m_client->setSelection(attribute.start, attribute.start + attribute.length);
    }
}
