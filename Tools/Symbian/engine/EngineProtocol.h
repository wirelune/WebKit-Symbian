// The interface between wkengine.exe, the WebKit engine as a server process,
// and a front end in another process (the Qt browser in ../qtbrowser).
//
// The engine draws the page into a global chunk it shares with the client,
// 32-bit pixels 0xAARRGGBB (premultiplied, opaque) with a stride of four
// times the view's width. The client posts EWkNotify and the engine completes
// it when something changed (an area of the chunk was painted, the title,
// the address, the progress, the focused field, a popup to show); the client
// then reads what it needs with the EWkGet* calls and posts EWkNotify again.
//
// Calls that act on the page (loading, input) are queued and completed at
// once; the engine runs them in order afterwards. alert(), confirm() and
// <select> wait inside the engine for EWkAnswerPopup.
//
// Only Symbian types and C++98: the Qt front end is built with GCCE 4.x.
#ifndef WKENGINE_PROTOCOL_H
#define WKENGINE_PROTOCOL_H

#include <e32std.h>

_LIT(KWkEngineServerName, "WebKitEngine");
_LIT(KWkEngineExe, "wkengine.exe");

const TInt KWkEngineVersionMajor = 1;
const TInt KWkEngineVersionMinor = 0;
const TInt KWkEngineVersionBuild = 0;

enum TWkEngineOp {
    // (width, height, largest dimension the view will have): the view, and
    // the chunk with its pixels, sized for the largest dimension squared so
    // that turning the screen needs no new chunk. Completes with the chunk's
    // handle (RChunk::SetReturnedHandle()).
    EWkCreateView = 1,
    // (width, height)
    EWkSetSize,
    // (const TDesC16& url). Text without a scheme is http://.
    EWkLoad,
    EWkReload,
    EWkStop,
    EWkGoBack,
    EWkGoForward,
    // (TBool focused)
    EWkSetFocused,
    // (TWkPointer, x, y) in view pixels. Taps are clicks, drags scroll.
    EWkPointer,
    // (dx, dy)
    EWkScrollBy,
    // (Windows virtual key code, TWkModifier flags, const TDesC16& text):
    // a key press and its release.
    EWkKey,
    // (anchor, focus) in the focused field.
    EWkSetSelection,
    // (const TDesC16& text, cursor): replaces the selection or the current
    // composition with marked text.
    EWkSetComposition,
    // (const TDesC16& text): replaces the composition (or the selection)
    // with text and ends the composition.
    EWkConfirmComposition,
    EWkCancelComposition,

    // (TPckg<TWkState>&)
    EWkGetState = 0x40,
    // (TDes16&): completes with the full length; the text is cut to fit.
    EWkGetTitle,
    EWkGetUrl,
    // (start, length, TDes16&): the focused field's text, completes with the
    // number of characters copied.
    EWkGetEditableText,
    // (TPckg<TWkPopup>&)
    EWkGetPopup,
    // (TDes16&): the popup's message, or a <select>'s items, each ended by
    // KWkPopupItemSeparator. Completes with the full length.
    EWkGetPopupText,
    // (result): the index chosen in a <select> (-1 to cancel), or 1/0 for a
    // confirm(); any value for an alert().
    EWkAnswerPopup,

    // (TPckg<TWkEvents>&), asynchronous. One may be outstanding.
    EWkNotify = 0x80,
    EWkCancelNotify,
};

enum TWkPointer {
    EWkPointerDown,
    EWkPointerMove,
    EWkPointerUp,
};

enum TWkModifier {
    EWkModifierShift = 1 << 0,
    EWkModifierControl = 1 << 1,
    EWkModifierAlt = 1 << 2,
};

// The kind of field with the focus (WebKitSymbian::InputType).
enum TWkInputType {
    EWkInputNone,
    EWkInputText,
    EWkInputPassword,
    EWkInputNumber,
    EWkInputTelephone,
    EWkInputEmail,
    EWkInputUrl,
    EWkInputSearch,
    EWkInputMultiLine,
};

enum TWkEventFlag {
    // TWkEvents::iDirty was painted in the chunk.
    EWkEventPaint = 1 << 0,
    EWkEventTitle = 1 << 1,
    EWkEventUrl = 1 << 2,
    // Loading, progress, back and forward.
    EWkEventProgress = 1 << 3,
    // The focus moved into or out of an editable field.
    EWkEventEditing = 1 << 4,
    // The focused field's text or selection changed.
    EWkEventSelection = 1 << 5,
    // A popup waits for EWkAnswerPopup.
    EWkEventPopup = 1 << 6,
    // The network connection failed (TWkState::iNetworkError).
    EWkEventNetwork = 1 << 7,
};

struct TWkEvents {
    TUint iFlags;
    TRect iDirty;
    // The view's size, which the chunk's pixels have now: a resize takes
    // effect in the engine after EWkSetSize has completed.
    TInt iWidth;
    TInt iHeight;
};

struct TWkState {
    TInt iWidth;
    TInt iHeight;
    TInt iStride;
    TBool iLoading;
    // 0 to 100.
    TInt iProgress;
    TBool iCanGoBack;
    TBool iCanGoForward;
    // TWkInputType, and the focused field's editing state.
    TInt iInputType;
    TInt iTextLength;
    TInt iMaxTextLength;
    TInt iSelectionAnchor;
    TInt iSelectionFocus;
    TRect iCaret;
    // The last network connection error, KErrNone once connected.
    TInt iNetworkError;
};

enum TWkPopupType {
    EWkPopupNone,
    EWkPopupAlert,
    EWkPopupConfirm,
    EWkPopupSelect,
};

struct TWkPopup {
    TInt iType;
    // For a <select>: the number of items and the selected one.
    TInt iCount;
    TInt iSelected;
    // The length of EWkGetPopupText's text.
    TInt iTextLength;
};

const TUint16 KWkPopupItemSeparator = 0x2029; // PARAGRAPH SEPARATOR

#endif // WKENGINE_PROTOCOL_H
