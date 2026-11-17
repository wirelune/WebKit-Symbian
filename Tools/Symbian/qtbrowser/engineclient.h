// The Qt side of wkengine.exe (../engine/EngineProtocol.h): starts the
// engine process, connects to it and turns its notifications into signals.
// Outside Symbian (the Qt Simulator) there is no engine; start() fails.
#ifndef ENGINECLIENT_H
#define ENGINECLIENT_H

#include <QObject>
#include <QImage>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

class EngineClientPrivate;

class EngineClient : public QObject
{
    Q_OBJECT
public:
    // TWkInputType
    enum InputType { InputNone, InputText, InputPassword, InputNumber, InputTelephone,
                     InputEmail, InputUrl, InputSearch, InputMultiLine };
    enum PointerType { PointerDown, PointerMove, PointerUp };
    enum Modifier { ShiftModifier = 1, ControlModifier = 2, AltModifier = 4 };
    enum PopupType { PopupNone, PopupAlert, PopupConfirm, PopupSelect };

    struct State {
        State();
        bool loading;
        int progress;
        bool canGoBack;
        bool canGoForward;
        int inputType;
        int textLength;
        int maxTextLength;
        int selectionAnchor;
        int selectionFocus;
        QRect caret;
        int networkError;
    };

    explicit EngineClient(QObject *parent = 0);
    ~EngineClient();

    // Starts the engine (or connects to a running one) with a view of
    // size; maxDimension is the longest side the view will ever have.
    // Emits ready() or failed().
    void start(const QSize &size, int maxDimension);
    bool isReady() const;

    // The page's pixels, shared with the engine: valid until the next
    // painted() changes the size. Empty until ready().
    QImage image() const;

    void setSize(const QSize &size);
    void load(const QString &url);
    void reload();
    void stop();
    void goBack();
    void goForward();
    void setFocused(bool focused);
    void pointer(PointerType type, const QPoint &position);
    void scrollBy(int dx, int dy);
    // A Windows virtual key code, Modifier flags and the key's text.
    void key(int windowsKeyCode, int modifiers, const QString &text);
    void setSelection(int anchor, int focus);
    void setComposition(const QString &text, int cursor);
    void confirmComposition(const QString &text);
    void cancelComposition();

    // From the engine, at the time of the call.
    State state() const;
    QString title() const;
    QString url() const;
    QString editableText(int start, int length) const;

    PopupType popupType() const;
    QString popupMessage() const;
    QStringList popupItems(int *selected) const;
    void answerPopup(int result);

signals:
    void ready();
    // A Symbian error code: the engine is missing or could not start.
    void failed(int error);
    // The engine process ended.
    void died(int reason);

    void painted(const QRect &rect);
    void titleChanged();
    void urlChanged();
    void progressChanged();
    void editingChanged();
    void selectionChanged();
    void popupRequested();
    void networkFailed(int error);

private:
    friend class EngineClientPrivate;
    EngineClientPrivate *d;
};

#endif // ENGINECLIENT_H
