#include "pagestore.h"

#include <QSettings>
#include <QStringList>
#include <QVariantMap>

namespace {

const int KHistoryMaxCount = 100;

QVariantMap page(const QString &url, const QString &title)
{
    QVariantMap map;
    map.insert(QLatin1String("url"), url);
    map.insert(QLatin1String("title"), title.isEmpty() ? url : title);
    return map;
}

QVariantList read(QSettings *settings, const QString &key)
{
    QVariantList pages;
    int count = settings->beginReadArray(key);
    for (int i = 0; i < count; ++i) {
        settings->setArrayIndex(i);
        QString url = settings->value(QLatin1String("url")).toString();
        if (!url.isEmpty())
            pages.append(page(url, settings->value(QLatin1String("title")).toString()));
    }
    settings->endArray();
    return pages;
}

} // namespace

PageStore::PageStore(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(QSettings::IniFormat, QSettings::UserScope,
          QLatin1String("symbian-webkit"), QLatin1String("qtbrowser"), this))
{
    m_bookmarks = read(m_settings, QLatin1String("bookmarks"));
    m_history = read(m_settings, QLatin1String("history"));
}

int PageStore::find(const QVariantList &pages, const QString &url)
{
    for (int i = 0; i < pages.count(); ++i) {
        if (pages.at(i).toMap().value(QLatin1String("url")).toString() == url)
            return i;
    }
    return -1;
}

void PageStore::save(const QString &key, const QVariantList &pages)
{
    m_settings->beginWriteArray(key, pages.count());
    for (int i = 0; i < pages.count(); ++i) {
        m_settings->setArrayIndex(i);
        QVariantMap map = pages.at(i).toMap();
        m_settings->setValue(QLatin1String("url"), map.value(QLatin1String("url")));
        m_settings->setValue(QLatin1String("title"), map.value(QLatin1String("title")));
    }
    m_settings->endArray();
    m_settings->sync();
}

bool PageStore::isBookmarked(const QString &url) const
{
    return find(m_bookmarks, url) >= 0;
}

void PageStore::addBookmark(const QString &url, const QString &title)
{
    if (url.isEmpty() || isBookmarked(url))
        return;
    m_bookmarks.append(page(url, title));
    save(QLatin1String("bookmarks"), m_bookmarks);
    emit bookmarksChanged();
}

void PageStore::removeBookmark(const QString &url)
{
    int index = find(m_bookmarks, url);
    if (index < 0)
        return;
    m_bookmarks.removeAt(index);
    save(QLatin1String("bookmarks"), m_bookmarks);
    emit bookmarksChanged();
}

void PageStore::visited(const QString &url, const QString &title)
{
    if (url.isEmpty() || url == QLatin1String("about:blank"))
        return;
    int index = find(m_history, url);
    if (index == 0 && title.isEmpty())
        return;
    QString known = index >= 0 ? m_history.at(index).toMap().value(QLatin1String("title")).toString() : QString();
    if (index >= 0)
        m_history.removeAt(index);
    m_history.prepend(page(url, title.isEmpty() ? known : title));
    while (m_history.count() > KHistoryMaxCount)
        m_history.removeLast();
    save(QLatin1String("history"), m_history);
    emit historyChanged();
}

void PageStore::clearHistory()
{
    m_history.clear();
    save(QLatin1String("history"), m_history);
    emit historyChanged();
}

QVariantList PageStore::suggestions(const QString &text, int maxCount) const
{
    QVariantList result;
    QStringList seen;
    QString filter = text.trimmed();
    QList<QVariantList> lists;
    lists << m_bookmarks << m_history;
    foreach (const QVariantList &pages, lists) {
        foreach (const QVariant &entry, pages) {
            if (result.count() >= maxCount)
                return result;
            QVariantMap map = entry.toMap();
            QString url = map.value(QLatin1String("url")).toString();
            QString title = map.value(QLatin1String("title")).toString();
            if (seen.contains(url))
                continue;
            if (!filter.isEmpty() && !url.contains(filter, Qt::CaseInsensitive)
                && !title.contains(filter, Qt::CaseInsensitive))
                continue;
            seen.append(url);
            result.append(map);
        }
    }
    return result;
}

QString PageStore::homePage() const
{
    return m_settings->value(QLatin1String("homePage"), QLatin1String("https://duckduckgo.com/lite/")).toString();
}
