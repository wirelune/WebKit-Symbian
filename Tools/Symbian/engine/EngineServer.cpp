// wkengine.exe: the Symbian port's WebView in a server process of its own,
// for a front end built with another toolchain (the Qt 4.7 browser in
// ../qtbrowser, which GCCE builds). See EngineProtocol.h for the protocol.
//
// One client: the process ends when its session closes, so a front end that
// exits or dies takes the engine with it.

#include "EngineProtocol.h"

#include "BrowserLog.h"
#include "MediaPlayers.h"
#include "NetworkConnection.h"
#include "PageList.h"
#include "RunLoopDriver.h"

#include <WebKitSymbian.h>
#include <bautils.h>
#include <e32base.h>
#include <f32file.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

using namespace WebKitSymbian;

namespace {

// Cookies, caches and the log; apart from the AVKON browser's C:/Data/WebKit,
// so that both can run.
const char KDataDirectory[] = "C:/Data/WebKitQt";
const TInt KMaxDimension = 2048;

TPtrC ToDesC(Text aText)
{
    return TPtrC(reinterpret_cast<const TText*>(aText.characters), aText.length);
}

Text ToText(const TDesC& aDes)
{
    return Text { reinterpret_cast<const uint16_t*>(aDes.Ptr()), aDes.Length() };
}

// Whether a URL needs the network connection: anything but a local file or
// a page the engine makes itself.
TBool IsNetworkUrl(const TDesC& aUrl)
{
    if (aUrl.Length() >= 2 && aUrl[1] == ':' && TChar(aUrl[0]).IsAlpha())
        return EFalse; // A drive path, C:/Data/...
    _LIT(KFile, "file:");
    _LIT(KAbout, "about:");
    _LIT(KData, "data:");
    _LIT(KJavaScript, "javascript:");
    const TDesC* local[] = { &KFile, &KAbout, &KData, &KJavaScript };
    for (const TDesC* scheme : local) {
        if (aUrl.Left(scheme->Length()).CompareF(*scheme) == 0)
            return EFalse;
    }
    return ETrue;
}

// <audio> plays in the engine; <video> needs a window, which the engine has
// none of, so video elements report that they can't play.
MediaPlayer* CreateAudioPlayer(void*, MediaPlayerClient& aClient, bool aHasVideo)
{
    return CreateMediaPlayer(nullptr, aClient, aHasVideo);
}

class CEngineServer : public CServer2 {
public:
    CEngineServer()
        : CServer2(EPriorityStandard, EUnsharableSessions)
    {
    }

    CSession2* NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const;

    // The engine serves one client and ends with its session.
    void SessionOpened() { ++iSessions; }
    void SessionClosed()
    {
        if (--iSessions <= 0) {
            BrowserLog("client gone, engine exiting");
            User::Exit(KErrNone);
        }
    }

    TInt Sessions() const { return iSessions; }

private:
    TInt iSessions { 0 };
};

class CEngineSession : public CSession2, public WebViewClient, public MNetworkConnectionObserver {
public:
    CEngineSession() = default;
    ~CEngineSession();

    void ConstructL();
    void ServiceL(const RMessage2& aMessage);

private:
    CEngineServer& EngineServer() { return *static_cast<CEngineServer*>(const_cast<CServer2*>(Server())); }

    // A call that acts on the page, run after its message is completed.
    struct TOperation {
        TInt iFunction;
        TInt iArgs[3];
        HBufC* iText;
    };

    void CreateViewL(const RMessage2& aMessage);
    void QueueL(const RMessage2& aMessage);
    static TInt RunOperations(TAny* aSelf);
    void Run(const TOperation& aOperation);
    void LoadL(const TDesC& aUrl);
    void LoadNow(const TDesC& aUrl);

    void GetStateL(const RMessage2& aMessage);
    void WriteTextL(const RMessage2& aMessage, TInt aIndex, const TDesC& aText);
    void GetEditableTextL(const RMessage2& aMessage);
    void GetPopupL(const RMessage2& aMessage);

    void AddEvents(TUint aFlags);
    void DeliverEvents();
    static TInt Paint(TAny* aSelf);
    void SchedulePaint();
    TRect ViewRect() const { return TRect(TSize(iWidth, iHeight)); }

    // Shows a popup in the client and waits for its answer.
    TInt RunPopup(TInt aType, const TDesC& aText, TInt aCount, TInt aSelected);

    // WebViewClient
    void didInvalidate(const WebKitSymbian::Rect&);
    void didScroll(const WebKitSymbian::Rect&, int dx, int dy);
    void didChangeTitle(Text);
    void didChangeURL(Text);
    void didChangeProgress(bool isLoading, double progress);
    void didChangeEditingState();
    void didChangeSelection();
    void runJavaScriptAlert(Text);
    bool runJavaScriptConfirm(Text);
    int runSelectPopup(const Text*, int count, int selectedIndex);

    // MNetworkConnectionObserver
    void NetworkConnectionReady(TInt aError);

    WebView* iWebView { nullptr };
    RChunk iChunk;
    TInt iChunkSize { 0 };
    TInt iWidth { 0 };
    TInt iHeight { 0 };

    RArray<TOperation> iOperations;
    CAsyncCallBack* iRunOperations { nullptr };
    TBool iRunning { EFalse };

    // Invalidated by the engine, not painted yet.
    TRect iDirty;
    // Painted, not reported to the client yet.
    TRect iPainted;
    CAsyncCallBack* iPaint { nullptr };

    TUint iEvents { 0 };
    RMessage2 iNotify;
    TBool iNotifyPending { EFalse };

    TInt iPopupType { EWkPopupNone };
    HBufC* iPopupText { nullptr };
    TInt iPopupCount { 0 };
    TInt iPopupSelected { -1 };
    TInt iPopupResult { -1 };
    CActiveSchedulerWait iPopupWait;

    CNetworkConnection* iNetwork { nullptr };
    HBufC* iPendingUrl { nullptr };
    TInt iNetworkError { KErrNone };

    HBufC* iTitle { nullptr };
    HBufC* iUrl { nullptr };
    TBool iLoading { EFalse };
    TInt iProgress { 0 };
};

CSession2* CEngineServer::NewSessionL(const TVersion& aVersion, const RMessage2&) const
{
    if (!User::QueryVersionSupported(TVersion(KWkEngineVersionMajor, KWkEngineVersionMinor, KWkEngineVersionBuild), aVersion))
        User::Leave(KErrNotSupported);
    if (iSessions)
        User::Leave(KErrInUse);
    CEngineSession* session = new (ELeave) CEngineSession;
    CleanupStack::PushL(session);
    session->ConstructL();
    CleanupStack::Pop(session);
    const_cast<CEngineServer*>(this)->SessionOpened();
    return session;
}

void CEngineSession::ConstructL()
{
    // Above the server's own priority: a queued call runs before the next
    // message is served, so that what the client asks next sees its effect.
    iRunOperations = new (ELeave) CAsyncCallBack(TCallBack(RunOperations, this), CActive::EPriorityHigh);
    // Below the engine's work and the client's calls, so that invalidations
    // gather into one paint.
    iPaint = new (ELeave) CAsyncCallBack(TCallBack(Paint, this), CActive::EPriorityLow);
    iNetwork = CNetworkConnection::NewL(*this);
    // Start the connection now, so the system's access point prompt (if
    // any) comes up once, while the first page is being typed.
    TRAPD(error, iNetwork->StartL());
    if (error)
        BrowserLog("network: cannot start (%d)", error);
}

CEngineSession::~CEngineSession()
{
    // The engine is not torn down: the process exits with the session.
    if (iNotifyPending)
        iNotify.Complete(KErrCancel);
    for (TInt i = 0; i < iOperations.Count(); ++i)
        User::Free(iOperations[i].iText);
    iOperations.Close();
    delete iRunOperations;
    delete iPaint;
    User::Free(iPopupText);
    User::Free(iPendingUrl);
    User::Free(iTitle);
    User::Free(iUrl);
    if (Server())
        EngineServer().SessionClosed();
}

void CEngineSession::ServiceL(const RMessage2& aMessage)
{
    switch (aMessage.Function()) {
    case EWkCreateView:
        CreateViewL(aMessage);
        return;
    case EWkNotify:
        if (iNotifyPending) {
            aMessage.Panic(KWkEngineServerName, 1);
            return;
        }
        iNotify = aMessage;
        iNotifyPending = ETrue;
        DeliverEvents();
        return;
    case EWkCancelNotify:
        if (iNotifyPending) {
            iNotifyPending = EFalse;
            iNotify.Complete(KErrCancel);
        }
        aMessage.Complete(KErrNone);
        return;
    case EWkGetState:
        GetStateL(aMessage);
        return;
    case EWkGetTitle:
        WriteTextL(aMessage, 0, iTitle ? static_cast<const TDesC&>(*iTitle) : KNullDesC());
        return;
    case EWkGetUrl:
        WriteTextL(aMessage, 0, iUrl ? static_cast<const TDesC&>(*iUrl) : KNullDesC());
        return;
    case EWkGetEditableText:
        GetEditableTextL(aMessage);
        return;
    case EWkGetPopup:
        GetPopupL(aMessage);
        return;
    case EWkGetPopupText:
        WriteTextL(aMessage, 0, iPopupText ? static_cast<const TDesC&>(*iPopupText) : KNullDesC());
        return;
    case EWkAnswerPopup:
        if (iPopupType != EWkPopupNone) {
            iPopupResult = aMessage.Int0();
            iPopupType = EWkPopupNone;
            if (iPopupWait.IsStarted())
                iPopupWait.AsyncStop();
        }
        aMessage.Complete(KErrNone);
        return;
    case EWkSetSize:
        if (aMessage.Int0() <= 0 || aMessage.Int1() <= 0 || aMessage.Int0() * aMessage.Int1() * 4 > iChunkSize) {
            aMessage.Complete(KErrArgument);
            return;
        }
        QueueL(aMessage);
        return;
    case EWkLoad:
    case EWkReload:
    case EWkStop:
    case EWkGoBack:
    case EWkGoForward:
    case EWkSetFocused:
    case EWkPointer:
    case EWkScrollBy:
    case EWkKey:
    case EWkSetSelection:
    case EWkSetComposition:
    case EWkConfirmComposition:
    case EWkCancelComposition:
        if (!iWebView) {
            aMessage.Complete(KErrNotReady);
            return;
        }
        QueueL(aMessage);
        return;
    default:
        aMessage.Panic(KWkEngineServerName, 2);
        return;
    }
}

void CEngineSession::CreateViewL(const RMessage2& aMessage)
{
    TInt width = aMessage.Int0();
    TInt height = aMessage.Int1();
    TInt dimension = Max(aMessage.Int2(), Max(width, height));
    if (iWebView || width <= 0 || height <= 0 || dimension > KMaxDimension) {
        aMessage.Complete(iWebView ? KErrAlreadyExists : KErrArgument);
        return;
    }
    iChunkSize = dimension * dimension * 4;
    User::LeaveIfError(iChunk.CreateGlobal(KNullDesC, iChunkSize, iChunkSize));
    // White until the first paint.
    Mem::Fill(iChunk.Base(), iChunkSize, 0xff);
    iWidth = width;
    iHeight = height;
    WebView::setMediaPlayerFactory(CreateAudioPlayer, nullptr);
    iWebView = WebView::create(*this, width, height);
    iWebView->setFocused(true);
    iDirty = ViewRect();
    SchedulePaint();
    BrowserLog("view %dx%d, chunk %d KB", width, height, iChunkSize / 1024);
    // The client gets its own handle to the chunk.
    aMessage.Complete(iChunk);
}

void CEngineSession::QueueL(const RMessage2& aMessage)
{
    TOperation operation;
    operation.iFunction = aMessage.Function();
    operation.iArgs[0] = aMessage.Int0();
    operation.iArgs[1] = aMessage.Int1();
    operation.iArgs[2] = aMessage.Int2();
    operation.iText = nullptr;
    TInt textIndex = -1;
    switch (operation.iFunction) {
    case EWkLoad:
    case EWkSetComposition:
    case EWkConfirmComposition:
        textIndex = 0;
        break;
    case EWkKey:
        textIndex = 2;
        break;
    default:
        break;
    }
    if (textIndex >= 0) {
        TInt length = aMessage.GetDesLengthL(textIndex);
        operation.iText = HBufC::NewL(Max(length, 1));
        TPtr text = operation.iText->Des();
        TRAPD(error, aMessage.ReadL(textIndex, text));
        if (!error)
            error = iOperations.Append(operation);
        if (error) {
            User::Free(operation.iText);
            User::Leave(error);
        }
    } else
        User::LeaveIfError(iOperations.Append(operation));
    aMessage.Complete(KErrNone);
    if (!iRunning)
        iRunOperations->CallBack();
}

TInt CEngineSession::RunOperations(TAny* aSelf)
{
    CEngineSession* self = static_cast<CEngineSession*>(aSelf);
    // A popup waits in a nested scheduler loop inside an operation; the ones
    // that come meanwhile run after it.
    if (self->iRunning)
        return 0;
    self->iRunning = ETrue;
    while (self->iOperations.Count()) {
        TOperation operation = self->iOperations[0];
        self->iOperations.Remove(0);
        self->Run(operation);
        User::Free(operation.iText);
    }
    self->iRunning = EFalse;
    return 0;
}

void CEngineSession::Run(const TOperation& aOperation)
{
    const TInt* args = aOperation.iArgs;
    TPtrC text = aOperation.iText ? TPtrC(*aOperation.iText) : TPtrC();
    switch (aOperation.iFunction) {
    case EWkSetSize:
        if (args[0] == iWidth && args[1] == iHeight)
            return;
        iWidth = args[0];
        iHeight = args[1];
        iWebView->setSize(iWidth, iHeight);
        // The stride changed: everything is painted again.
        iDirty = ViewRect();
        SchedulePaint();
        return;
    case EWkLoad:
        TRAP_IGNORE(LoadL(text));
        return;
    case EWkReload:
        iWebView->reload();
        return;
    case EWkStop:
        iWebView->stop();
        return;
    case EWkGoBack:
        if (iWebView->canGoBack())
            iWebView->goBack();
        return;
    case EWkGoForward:
        if (iWebView->canGoForward())
            iWebView->goForward();
        return;
    case EWkSetFocused:
        iWebView->setFocused(args[0]);
        return;
    case EWkPointer: {
        PointerEvent type = args[0] == EWkPointerDown ? PointerEvent::Down
            : args[0] == EWkPointerMove ? PointerEvent::Move : PointerEvent::Up;
        iWebView->handlePointer(type, args[1], args[2]);
        return;
    }
    case EWkScrollBy:
        iWebView->scrollBy(args[0], args[1]);
        return;
    case EWkKey: {
        unsigned modifiers = 0;
        if (args[1] & EWkModifierShift)
            modifiers |= ShiftKey;
        if (args[1] & EWkModifierControl)
            modifiers |= ControlKey;
        if (args[1] & EWkModifierAlt)
            modifiers |= AltKey;
        iWebView->handleKey(true, args[0], ToText(text), modifiers);
        iWebView->handleKey(false, args[0], Text { }, modifiers);
        return;
    }
    case EWkSetSelection:
        iWebView->setSelection(args[0], args[1]);
        return;
    case EWkSetComposition:
        iWebView->setComposition(ToText(text), args[1]);
        return;
    case EWkConfirmComposition:
        iWebView->confirmComposition(ToText(text));
        return;
    case EWkCancelComposition:
        iWebView->cancelComposition();
        return;
    default:
        return;
    }
}

void CEngineSession::LoadL(const TDesC& aUrl)
{
    if (!IsNetworkUrl(aUrl) || iNetwork->IsReady()) {
        LoadNow(aUrl);
        return;
    }
    // Load once the connection is up, or has failed (the page then shows
    // the error).
    HBufC* url = aUrl.AllocL();
    User::Free(iPendingUrl);
    iPendingUrl = url;
    iNetwork->StartL();
}

void CEngineSession::LoadNow(const TDesC& aUrl)
{
    HBufC8* utf8 = nullptr;
    TRAPD(error, utf8 = ToUtf8LC(aUrl); CleanupStack::Pop(utf8));
    if (error)
        return;
    BrowserLog("load %s", reinterpret_cast<const char*>(utf8->Ptr()));
    iWebView->load(reinterpret_cast<const char*>(utf8->Ptr()));
    User::Free(utf8);
}

void CEngineSession::NetworkConnectionReady(TInt aError)
{
    iNetworkError = aError;
    if (aError)
        AddEvents(EWkEventNetwork);
    if (!iPendingUrl)
        return;
    HBufC* url = iPendingUrl;
    iPendingUrl = nullptr;
    if (iWebView)
        LoadNow(*url);
    User::Free(url);
}

void CEngineSession::GetStateL(const RMessage2& aMessage)
{
    TWkState state;
    Mem::FillZ(&state, sizeof(state));
    state.iWidth = iWidth;
    state.iHeight = iHeight;
    state.iStride = iWidth * 4;
    state.iLoading = iLoading;
    state.iProgress = iProgress;
    state.iNetworkError = iNetworkError;
    if (iWebView) {
        state.iCanGoBack = iWebView->canGoBack();
        state.iCanGoForward = iWebView->canGoForward();
        state.iInputType = static_cast<TInt>(iWebView->focusedInputType());
        if (state.iInputType != EWkInputNone) {
            state.iTextLength = iWebView->editableTextLength();
            state.iMaxTextLength = iWebView->maximumEditableTextLength();
            int anchor = 0;
            int focus = 0;
            iWebView->selection(anchor, focus);
            state.iSelectionAnchor = anchor;
            state.iSelectionFocus = focus;
            WebKitSymbian::Rect caret = iWebView->caretRect();
            state.iCaret = TRect(TPoint(caret.x, caret.y), TSize(caret.width, caret.height));
        }
    }
    TPckgC<TWkState> package(state);
    aMessage.WriteL(0, package);
    aMessage.Complete(KErrNone);
}

void CEngineSession::WriteTextL(const RMessage2& aMessage, TInt aIndex, const TDesC& aText)
{
    TInt room = aMessage.GetDesMaxLengthL(aIndex);
    aMessage.WriteL(aIndex, aText.Left(room));
    aMessage.Complete(aText.Length());
}

void CEngineSession::GetEditableTextL(const RMessage2& aMessage)
{
    if (!iWebView || iWebView->focusedInputType() == InputType::None) {
        aMessage.WriteL(2, KNullDesC);
        aMessage.Complete(0);
        return;
    }
    TInt start = Max(aMessage.Int0(), 0);
    TInt length = Min(aMessage.Int1(), aMessage.GetDesMaxLengthL(2));
    if (length <= 0) {
        aMessage.Complete(0);
        return;
    }
    HBufC* buffer = HBufC::NewLC(length);
    TPtr text = buffer->Des();
    text.SetLength(length);
    int copied = iWebView->copyEditableText(start, length, reinterpret_cast<uint16_t*>(&text[0]));
    text.SetLength(Max(copied, 0));
    aMessage.WriteL(2, text);
    CleanupStack::Pop(buffer);
    User::Free(buffer);
    aMessage.Complete(text.Length());
}

void CEngineSession::GetPopupL(const RMessage2& aMessage)
{
    TWkPopup popup;
    popup.iType = iPopupType;
    popup.iCount = iPopupCount;
    popup.iSelected = iPopupSelected;
    popup.iTextLength = iPopupText ? iPopupText->Length() : 0;
    TPckgC<TWkPopup> package(popup);
    aMessage.WriteL(0, package);
    aMessage.Complete(KErrNone);
}

void CEngineSession::AddEvents(TUint aFlags)
{
    iEvents |= aFlags;
    DeliverEvents();
}

void CEngineSession::DeliverEvents()
{
    if (!iNotifyPending || !iEvents)
        return;
    TWkEvents events;
    events.iFlags = iEvents;
    events.iDirty = iPainted;
    events.iWidth = iWidth;
    events.iHeight = iHeight;
    TPckgC<TWkEvents> package(events);
    TInt error = iNotify.Write(0, package);
    iEvents = 0;
    iPainted = TRect();
    iNotifyPending = EFalse;
    iNotify.Complete(error);
}

void CEngineSession::SchedulePaint()
{
    if (!iPaint->IsActive())
        iPaint->CallBack();
}

TInt CEngineSession::Paint(TAny* aSelf)
{
    CEngineSession* self = static_cast<CEngineSession*>(aSelf);
    TRect dirty = self->iDirty;
    dirty.Intersection(self->ViewRect());
    self->iDirty = TRect();
    if (!dirty.IsEmpty() && self->iWebView) {
        WebKitSymbian::Rect area { dirty.iTl.iX, dirty.iTl.iY, dirty.Width(), dirty.Height() };
        self->iWebView->paint(self->iChunk.Base(), self->iWidth * 4, area);
        if (self->iPainted.IsEmpty())
            self->iPainted = dirty;
        else
            self->iPainted.BoundingRect(dirty);
    }
    if (!self->iPainted.IsEmpty())
        self->AddEvents(EWkEventPaint);
    return 0;
}

TInt CEngineSession::RunPopup(TInt aType, const TDesC& aText, TInt aCount, TInt aSelected)
{
    // One at a time: a page's popup while another is open is cancelled.
    if (iPopupType != EWkPopupNone || iPopupWait.IsStarted())
        return -1;
    HBufC* text = aText.Alloc();
    if (!text)
        return -1;
    User::Free(iPopupText);
    iPopupText = text;
    iPopupType = aType;
    iPopupCount = aCount;
    iPopupSelected = aSelected;
    iPopupResult = -1;
    AddEvents(EWkEventPopup);
    iPopupWait.Start();
    iPopupType = EWkPopupNone;
    User::Free(iPopupText);
    iPopupText = nullptr;
    return iPopupResult;
}

// WebViewClient. These run inside the engine, which must not be left
// through, so every leave stops here.

void CEngineSession::didInvalidate(const WebKitSymbian::Rect& aRect)
{
    TRect rect(TPoint(aRect.x, aRect.y), TSize(aRect.width, aRect.height));
    rect.Intersection(ViewRect());
    if (rect.IsEmpty())
        return;
    if (iDirty.IsEmpty())
        iDirty = rect;
    else
        iDirty.BoundingRect(rect);
    SchedulePaint();
}

// Moves the page's pixels in the chunk rather than painting them again; the
// client is told once the area that the scroll uncovered is painted too.
void CEngineSession::didScroll(const WebKitSymbian::Rect& aArea, int aDx, int aDy)
{
    TRect area(TPoint(aArea.x, aArea.y), TSize(aArea.width, aArea.height));
    area.Intersection(ViewRect());
    TPoint delta(aDx, aDy);
    TRect target(area);
    target.Move(delta);
    target.Intersection(area);
    if (target.IsEmpty()) {
        didInvalidate(aArea);
        return;
    }
    // Pixels waiting to be painted again move as well.
    TRect dirty(iDirty);
    dirty.Intersection(area);
    if (!dirty.IsEmpty()) {
        dirty.Move(delta);
        dirty.Intersection(area);
        if (!dirty.IsEmpty())
            iDirty.BoundingRect(dirty);
    }

    TRect source(target);
    source.Move(-delta);
    TUint8* pixels = iChunk.Base();
    TInt stride = iWidth * 4;
    TInt length = target.Width() * 4;
    TInt rows = target.Height();
    // Rows are copied away from the side they move to, so none is
    // overwritten before it is copied.
    TInt step = aDy > 0 ? -stride : stride;
    TInt first = aDy > 0 ? rows - 1 : 0;
    TUint8* to = pixels + (target.iTl.iY + first) * stride + target.iTl.iX * 4;
    const TUint8* from = pixels + (source.iTl.iY + first) * stride + source.iTl.iX * 4;
    for (TInt row = 0; row < rows; ++row, to += step, from += step)
        Mem::Move(to, from, length);
    if (iPainted.IsEmpty())
        iPainted = area;
    else
        iPainted.BoundingRect(area);
    SchedulePaint();
}

void CEngineSession::didChangeTitle(Text aTitle)
{
    HBufC* title = ToDesC(aTitle).Alloc();
    if (!title)
        return;
    User::Free(iTitle);
    iTitle = title;
    AddEvents(EWkEventTitle);
}

void CEngineSession::didChangeURL(Text aUrl)
{
    HBufC* url = ToDesC(aUrl).Alloc();
    if (!url)
        return;
    User::Free(iUrl);
    iUrl = url;
    AddEvents(EWkEventUrl);
}

void CEngineSession::didChangeProgress(bool aIsLoading, double aProgress)
{
    iLoading = aIsLoading;
    iProgress = static_cast<TInt>(aProgress * 100);
    AddEvents(EWkEventProgress);
}

void CEngineSession::didChangeEditingState()
{
    AddEvents(EWkEventEditing);
}

void CEngineSession::didChangeSelection()
{
    AddEvents(EWkEventSelection);
}

void CEngineSession::runJavaScriptAlert(Text aMessage)
{
    RunPopup(EWkPopupAlert, ToDesC(aMessage), 0, -1);
}

bool CEngineSession::runJavaScriptConfirm(Text aMessage)
{
    return RunPopup(EWkPopupConfirm, ToDesC(aMessage), 0, -1) > 0;
}

int CEngineSession::runSelectPopup(const Text* aItems, int aCount, int aSelected)
{
    if (aCount <= 0)
        return -1;
    TInt length = 0;
    for (TInt i = 0; i < aCount; ++i)
        length += aItems[i].length + 1;
    HBufC* items = HBufC::New(length);
    if (!items)
        return -1;
    TPtr text = items->Des();
    for (TInt i = 0; i < aCount; ++i) {
        text.Append(ToDesC(aItems[i]));
        text.Append(TChar(KWkPopupItemSeparator));
    }
    TInt index = RunPopup(EWkPopupSelect, text, aCount, aSelected);
    User::Free(items);
    return index >= 0 && index < aCount ? index : -1;
}

void RunServerL()
{
    // curl's own trace goes to C:\Data\WebKitQt\curl.txt while
    // C:\Data\WebKitQt\curl-verbose exists. The engine reads the settings
    // when it starts.
    RFs fs;
    User::LeaveIfError(fs.Connect());
    TBool curlTrace = BaflUtils::FileExists(fs, _L("C:\\Data\\WebKitQt\\curl-verbose"));
    fs.Close();
    if (curlTrace) {
        remove("C:/Data/WebKitQt/curl.txt");
        setenv("DEBUG_CURL", "1", 1);
        setenv("CURL_LOG_FILE", "C:/Data/WebKitQt/curl.txt", 1);
    }
    // The log first, so that a failure while the engine initializes shows.
    mkdir(KDataDirectory, 0777);
    BrowserLogOpen(KDataDirectory);
    BrowserLog("WebKit engine starting");
    WebView::initializeProcess(KDataDirectory);
    BrowserLog("engine initialized");
    if (curlTrace)
        BrowserThreadWatchStart();

    CRunLoopDriver* runLoop = CRunLoopDriver::NewL();
    CleanupStack::PushL(runLoop);
    CEngineServer* server = new (ELeave) CEngineServer;
    CleanupStack::PushL(server);
    server->StartL(KWkEngineServerName);
    RProcess::Rendezvous(KErrNone);
    CActiveScheduler::Start();
    CleanupStack::PopAndDestroy(2, runLoop);
}

} // namespace

int main()
{
    // The SDK's startup has made the cleanup stack; the engine's timers and
    // the server need an active scheduler.
    CActiveScheduler* scheduler = new CActiveScheduler;
    if (!scheduler)
        return KErrNoMemory;
    CActiveScheduler::Install(scheduler);
    TRAPD(error, RunServerL());
    if (error) {
        BrowserLog("engine: cannot start (%d)", error);
        RProcess::Rendezvous(error);
    }
    delete scheduler;
    return error;
}
