#include "engineclient.h"

#include "../engine/EngineProtocol.h"

#include <QtGlobal>
#include <bafl/sysutil.h>
#include <e32base.h>
#include <e32std.h>

namespace {

TPtrC16 toDesC(const QString &text)
{
    return TPtrC16(reinterpret_cast<const TUint16 *>(text.utf16()), text.length());
}

QString toQString(const TDesC16 &text)
{
    return QString::fromUtf16(text.Ptr(), text.Length());
}

QRect toQRect(const TRect &rect)
{
    return QRect(rect.iTl.iX, rect.iTl.iY, rect.Width(), rect.Height());
}

class REngineSession : public RSessionBase
{
public:
    TInt Connect()
    {
        // Two messages at once: the notification and a call.
        return CreateSession(KWkEngineServerName,
            TVersion(KWkEngineVersionMajor, KWkEngineVersionMinor, KWkEngineVersionBuild), 4);
    }
    TInt Call(TInt function, const TIpcArgs &args = TIpcArgs()) const
    {
        return SendReceive(function, args);
    }
    void CallAsync(TInt function, const TIpcArgs &args, TRequestStatus &status) const
    {
        SendReceive(function, args, status);
    }
};

// For the log: the firmware, and whether the system can load each of the
// port's executables. They are created and killed, never run.
void logEnvironment()
{
    TBuf<KSysUtilVersionTextLength> version;
    if (SysUtil::GetSWVersion(version) == KErrNone) {
        for (int i = 0; i < version.Length(); ++i) {
            if (version[i] < ' ')
                version[i] = ' ';
        }
        qDebug("firmware: %s", qPrintable(toQString(version)));
    }
    _LIT(KJsc, "jsc.exe");
    _LIT(KBrowser, "browser.exe");
    const TDesC *const names[] = { &KJsc, &KBrowser, &KWkEngineExe };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const TDesC &name = *names[i];
        RProcess process;
        TInt error = process.Create(name, KNullDesC);
        if (error == KErrNone) {
            TFileName path = process.FileName();
            qDebug("load test %s: ok, %s", qPrintable(toQString(name)), qPrintable(toQString(path)));
            process.Kill(KErrNone);
            process.Close();
        } else {
            qDebug("load test %s: %d", qPrintable(toQString(name)), error);
        }
    }
}

} // namespace

// Waits for the engine process to start (its rendezvous), then for
// notifications.
class EngineClientPrivate : public CActive
{
public:
    EngineClientPrivate(EngineClient *q)
        : CActive(EPriorityStandard)
        , q(q)
        , starting(false)
        , connected(false)
        , width(0)
        , height(0)
        , maxDimension(0)
    {
        CActiveScheduler::Add(this);
    }

    ~EngineClientPrivate()
    {
        Cancel();
        if (starting)
            process.Close();
        chunk.Close();
        session.Close();
    }

    void start(const QSize &size, int dimension);
    void connect();
    void post();
    TInt call(TInt function, const TIpcArgs &args = TIpcArgs()) const
    {
        if (!connected)
            return KErrNotReady;
        return session.Call(function, args);
    }
    void dispatch(TUint flags, const QRect &dirty);

    void RunL();
    void DoCancel();
    TInt RunError(TInt) { return KErrNone; }

    EngineClient *q;
    REngineSession session;
    RChunk chunk;
    RProcess process;
    bool starting;
    bool connected;
    int width;
    int height;
    int maxDimension;
    TPckgBuf<TWkEvents> events;
};

void EngineClientPrivate::start(const QSize &size, int dimension)
{
    width = size.width();
    height = size.height();
    maxDimension = dimension;
    logEnvironment();
    // An engine that runs already (a front end that died left it) serves
    // one client only; it ends once that one is gone.
    TInt error = process.Create(KWkEngineExe, KNullDesC);
    qDebug("engine: create wkengine.exe: %d", error);
    if (error) {
        emit q->failed(error);
        return;
    }
    starting = true;
    iStatus = KRequestPending;
    process.Rendezvous(iStatus);
    SetActive();
    process.Resume();
}

void EngineClientPrivate::connect()
{
    TInt error = session.Connect();
    qDebug("engine: connect %d", error);
    if (error) {
        emit q->failed(error);
        return;
    }
    TInt handle = session.Call(EWkCreateView, TIpcArgs(width, height, maxDimension));
    qDebug("engine: view %dx%d: %d", width, height, handle);
    if (handle < 0) {
        session.Close();
        emit q->failed(handle);
        return;
    }
    chunk.SetReturnedHandle(handle);
    connected = true;
    post();
    emit q->ready();
}

void EngineClientPrivate::post()
{
    iStatus = KRequestPending;
    session.CallAsync(EWkNotify, TIpcArgs(&events), iStatus);
    SetActive();
}

void EngineClientPrivate::RunL()
{
    TInt status = iStatus.Int();
    if (starting) {
        starting = false;
        TBool alive = process.ExitType() == EExitPending;
        if (!alive) {
            TExitCategoryName category = process.ExitCategory();
            QString name = toQString(category);
            qWarning("engine: exited at start: type %d, %s %d", process.ExitType(),
                qPrintable(name), process.ExitReason());
        }
        qDebug("engine: rendezvous %d", status);
        process.Close();
        // KErrAlreadyExists: another engine has the server name.
        if (status != KErrNone && (status != KErrAlreadyExists || alive)) {
            emit q->failed(status);
            return;
        }
        connect();
        return;
    }
    if (status < 0) {
        qWarning("engine: notification failed %d", status);
        connected = false;
        emit q->died(status);
        return;
    }
    const TWkEvents &e = events();
    width = e.iWidth;
    height = e.iHeight;
    // Signals run QML; nothing may leave the active scheduler from here.
    try {
        dispatch(e.iFlags, toQRect(e.iDirty));
    } catch (...) {
    }
    if (connected && !IsActive())
        post();
}

void EngineClientPrivate::dispatch(TUint flags, const QRect &dirty)
{
    if (flags & EWkEventProgress)
        emit q->progressChanged();
    if (flags & EWkEventUrl)
        emit q->urlChanged();
    if (flags & EWkEventTitle)
        emit q->titleChanged();
    if (flags & EWkEventEditing)
        emit q->editingChanged();
    if (flags & EWkEventSelection)
        emit q->selectionChanged();
    if (flags & EWkEventPaint)
        emit q->painted(dirty);
    if (flags & EWkEventNetwork)
        emit q->networkFailed(q->state().networkError);
    if (flags & EWkEventPopup)
        emit q->popupRequested();
}

void EngineClientPrivate::DoCancel()
{
    if (starting) {
        process.RendezvousCancel(iStatus);
        return;
    }
    session.Call(EWkCancelNotify);
}

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
    , d(new EngineClientPrivate(this))
{
}

EngineClient::~EngineClient()
{
    delete d;
}

void EngineClient::start(const QSize &size, int maxDimension)
{
    if (d->starting || d->connected)
        return;
    d->start(size.expandedTo(QSize(1, 1)), maxDimension);
}

bool EngineClient::isReady() const
{
    return d->connected;
}

QImage EngineClient::image() const
{
    if (!d->connected || d->width <= 0 || d->height <= 0)
        return QImage();
    // Opaque pixels: drawn without blending.
    return QImage(const_cast<const uchar *>(d->chunk.Base()), d->width, d->height, d->width * 4,
        QImage::Format_RGB32);
}

void EngineClient::setSize(const QSize &size)
{
    if (size.width() > 0 && size.height() > 0)
        d->call(EWkSetSize, TIpcArgs(size.width(), size.height()));
}

void EngineClient::load(const QString &url)
{
    TPtrC16 text = toDesC(url);
    d->call(EWkLoad, TIpcArgs(&text));
}

void EngineClient::reload()
{
    d->call(EWkReload);
}

void EngineClient::stop()
{
    d->call(EWkStop);
}

void EngineClient::goBack()
{
    d->call(EWkGoBack);
}

void EngineClient::goForward()
{
    d->call(EWkGoForward);
}

void EngineClient::setFocused(bool focused)
{
    d->call(EWkSetFocused, TIpcArgs(focused ? 1 : 0));
}

void EngineClient::pointer(PointerType type, const QPoint &position)
{
    d->call(EWkPointer, TIpcArgs(type, position.x(), position.y()));
}

void EngineClient::scrollBy(int dx, int dy)
{
    d->call(EWkScrollBy, TIpcArgs(dx, dy));
}

void EngineClient::key(int windowsKeyCode, int modifiers, const QString &text)
{
    TPtrC16 characters = toDesC(text);
    d->call(EWkKey, TIpcArgs(windowsKeyCode, modifiers, &characters));
}

void EngineClient::setSelection(int anchor, int focus)
{
    d->call(EWkSetSelection, TIpcArgs(anchor, focus));
}

void EngineClient::setComposition(const QString &text, int cursor)
{
    TPtrC16 characters = toDesC(text);
    d->call(EWkSetComposition, TIpcArgs(&characters, cursor));
}

void EngineClient::confirmComposition(const QString &text)
{
    TPtrC16 characters = toDesC(text);
    d->call(EWkConfirmComposition, TIpcArgs(&characters));
}

void EngineClient::cancelComposition()
{
    d->call(EWkCancelComposition);
}

EngineClient::State EngineClient::state() const
{
    State result;
    TPckgBuf<TWkState> package;
    if (d->call(EWkGetState, TIpcArgs(&package)) != KErrNone)
        return result;
    const TWkState &s = package();
    result.loading = s.iLoading;
    result.progress = s.iProgress;
    result.canGoBack = s.iCanGoBack;
    result.canGoForward = s.iCanGoForward;
    result.inputType = s.iInputType;
    result.textLength = s.iTextLength;
    result.maxTextLength = s.iMaxTextLength;
    result.selectionAnchor = s.iSelectionAnchor;
    result.selectionFocus = s.iSelectionFocus;
    result.caret = toQRect(s.iCaret);
    result.networkError = s.iNetworkError;
    return result;
}

static QString readText(const EngineClientPrivate *d, TInt function, int initialLength)
{
    int length = initialLength;
    for (int attempt = 0; attempt < 2; ++attempt) {
        HBufC16 *buffer = HBufC16::New(length);
        if (!buffer)
            return QString();
        TPtr16 text = buffer->Des();
        TInt full = d->call(function, TIpcArgs(&text));
        QString result = full >= 0 ? toQString(text) : QString();
        delete buffer;
        if (full <= length)
            return result;
        length = full;
    }
    return QString();
}

QString EngineClient::title() const
{
    return readText(d, EWkGetTitle, 256);
}

QString EngineClient::url() const
{
    return readText(d, EWkGetUrl, 1024);
}

QString EngineClient::editableText(int start, int length) const
{
    if (length <= 0)
        return QString();
    HBufC16 *buffer = HBufC16::New(length);
    if (!buffer)
        return QString();
    TPtr16 text = buffer->Des();
    TInt copied = d->call(EWkGetEditableText, TIpcArgs(start, length, &text));
    QString result = copied > 0 ? toQString(text) : QString();
    delete buffer;
    return result;
}

EngineClient::PopupType EngineClient::popupType() const
{
    TPckgBuf<TWkPopup> package;
    if (d->call(EWkGetPopup, TIpcArgs(&package)) != KErrNone)
        return PopupNone;
    return static_cast<PopupType>(package().iType);
}

QString EngineClient::popupMessage() const
{
    return readText(d, EWkGetPopupText, 512);
}

QStringList EngineClient::popupItems(int *selected) const
{
    TPckgBuf<TWkPopup> package;
    if (d->call(EWkGetPopup, TIpcArgs(&package)) != KErrNone)
        return QStringList();
    if (selected)
        *selected = package().iSelected;
    QString text = readText(d, EWkGetPopupText, package().iTextLength + 1);
    QStringList items = text.split(QChar(KWkPopupItemSeparator));
    // Each item ends with the separator: the last part is empty.
    if (!items.isEmpty() && items.last().isEmpty())
        items.removeLast();
    return items;
}

void EngineClient::answerPopup(int result)
{
    d->call(EWkAnswerPopup, TIpcArgs(result));
}
