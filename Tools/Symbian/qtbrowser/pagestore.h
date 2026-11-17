// The bookmarks and the history, kept with QSettings. Each list holds
// {url, title} objects for QML, the newest history entry first.
#ifndef PAGESTORE_H
#define PAGESTORE_H

#include <QObject>
#include <QVariantList>

class QSettings;

class PageStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
public:
    explicit PageStore(QObject *parent = 0);

    QVariantList bookmarks() const { return m_bookmarks; }
    QVariantList history() const { return m_history; }

    Q_INVOKABLE bool isBookmarked(const QString &url) const;
    Q_INVOKABLE void addBookmark(const QString &url, const QString &title);
    Q_INVOKABLE void removeBookmark(const QString &url);
    // A page was shown; a title that comes later updates its entry.
    Q_INVOKABLE void visited(const QString &url, const QString &title);
    Q_INVOKABLE void clearHistory();
    // Pages from both lists whose address or title contains text.
    Q_INVOKABLE QVariantList suggestions(const QString &text, int maxCount) const;

    Q_INVOKABLE QString homePage() const;

signals:
    void bookmarksChanged();
    void historyChanged();

private:
    static int find(const QVariantList &pages, const QString &url);
    void save(const QString &key, const QVariantList &pages);

    QSettings *m_settings;
    QVariantList m_bookmarks;
    QVariantList m_history;
};

#endif // PAGESTORE_H
