// WebPage, the QML element that shows a page of wkengine.exe: it paints the
// engine's pixels, sends it touches and keys, and is the input method's
// editor for the field that has the focus in the page.
#ifndef WEBPAGEITEM_H
#define WEBPAGEITEM_H

#include "engineclient.h"

#include <QDeclarativeItem>
#include <QStringList>

class QTimer;

class WebPageItem : public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(QString url READ url NOTIFY urlChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY progressChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY progressChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY progressChanged)
    // starting, ready, failed (the engine is missing) or died.
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int error READ error NOTIFY statusChanged)
    // The URL to load once the engine is ready.
    Q_PROPERTY(QString initialUrl READ initialUrl WRITE setInitialUrl)
public:
    explicit WebPageItem(QDeclarativeItem *parent = 0);
    ~WebPageItem();

    QString url() const { return m_url; }
    QString title() const { return m_title; }
    bool loading() const { return m_state.loading; }
    int progress() const { return m_state.progress; }
    bool canGoBack() const { return m_state.canGoBack; }
    bool canGoForward() const { return m_state.canGoForward; }
    QString status() const { return m_status; }
    int error() const { return m_error; }
    QString initialUrl() const { return m_initialUrl; }
    void setInitialUrl(const QString &url) { m_initialUrl = url; }

    Q_INVOKABLE void load(const QString &url);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    // Starts the engine again after it died.
    Q_INVOKABLE void restart();
    // The answer to alertRequested() (any value), confirmRequested()
    // (true/false as 1/0) or selectRequested() (the index, -1 to cancel).
    Q_INVOKABLE void answerPopup(int result);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const;

signals:
    void urlChanged();
    void titleChanged();
    void progressChanged();
    void statusChanged();
    void alertRequested(const QString &message);
    void confirmRequested(const QString &message);
    void selectRequested(const QStringList &items, int selected);
    void networkFailed(int error);
    // The page was touched.
    void pressed();

protected:
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);
    void inputMethodEvent(QInputMethodEvent *event);

private slots:
    void startEngine();
    void onReady();
    void onFailed(int error);
    void onDied(int reason);
    void onPainted(const QRect &rect);
    void onTitleChanged();
    void onUrlChanged();
    void onProgressChanged();
    void onEditingChanged();
    void onSelectionChanged();
    void onPopupRequested();
    void sendSize();

private:
    void connectClient();
    void setStatus(const QString &status, int error);
    void updateInputMethod();
    void showInputPanel(bool show);
    QSize viewSize() const;

    EngineClient *m_client;
    EngineClient::State m_state;
    QString m_url;
    QString m_title;
    QString m_status;
    int m_error;
    QString m_initialUrl;
    QTimer *m_resizeTimer;
    QPointF m_pressPosition;
    bool m_tapped;
    bool m_editable;
    bool m_composing;
    // Where the composition starts in the field, and its text.
    int m_compositionStart;
    QString m_composition;
};

#endif // WEBPAGEITEM_H
