#include "news.h"

#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSysInfo>
#include <QTimer>
#include <QXmlStreamReader>

#include <algorithm>

namespace Kyu
{
namespace
{
QHash<QString, QString> keyValues(const QString &path, QChar separator)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return values;
    }
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int at = line.indexOf(separator);
        if (at > 0) {
            QString value = line.mid(at + 1).trimmed();
            if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))) {
                value = value.mid(1, value.size() - 2);
            }
            values.insert(line.left(at).trimmed(), value);
        }
    }
    return values;
}

QDateTime parseDate(const QString &text)
{
    QDateTime dt = QDateTime::fromString(text.trimmed(), Qt::RFC2822Date);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(text.trimmed(), Qt::ISODate);
    }
    return dt;
}
}

NewsSource newsSourceFor(const QString &osReleasePath, const QString &mirrorsConfPath, const QString &cpuArchitecture)
{
    const auto os = keyValues(osReleasePath, QLatin1Char('='));
    const QString id = os.value(QStringLiteral("ID"));
    const QStringList like = os.value(QStringLiteral("ID_LIKE")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // Manjaro ARM's os-release says ID=manjaro-arm (ID_LIKE "manjaro arch").
    if (id == QLatin1String("manjaro") || id == QLatin1String("manjaro-arm") || like.contains(QLatin1String("manjaro"))) {
        QString branch = keyValues(mirrorsConfPath, QLatin1Char('=')).value(QStringLiteral("Branch"), QStringLiteral("stable"));
        if (branch != QLatin1String("testing") && branch != QLatin1String("unstable")) {
            branch = QStringLiteral("stable");
        }
        // Discourse category ids, as in Manjaro's own news app
        // (github.com/oguzkaganeren/manjaro-app, NewsScreen.tsx).
        const QString cpu = cpuArchitecture.isEmpty() ? QSysInfo::currentCpuArchitecture() : cpuArchitecture;
        const bool arm = id == QLatin1String("manjaro-arm") || cpu == QLatin1String("arm64");
        const int id = arm ? (branch == QLatin1String("testing") ? 80 : branch == QLatin1String("unstable") ? 81 : 79)
                           : (branch == QLatin1String("testing") ? 13 : branch == QLatin1String("unstable") ? 15 : 12);
        const QString page = QStringLiteral("https://forum.manjaro.org/c/%1/%2-updates/%3")
                                 .arg(arm ? QStringLiteral("arm") : QStringLiteral("announcements"), branch)
                                 .arg(id);
        return {QStringLiteral("Manjaro %1%2 updates").arg(arm ? QStringLiteral("ARM ") : QString(), branch), page + QStringLiteral(".rss"), page};
    }
    if (id == QLatin1String("arch") || like.contains(QLatin1String("arch"))) {
        return {QStringLiteral("Arch Linux news"), QStringLiteral("https://archlinux.org/feeds/news/"), QStringLiteral("https://archlinux.org/news/")};
    }
    return {};
}

QDateTime lastFullUpgrade(const QString &pacmanLogPath)
{
    QFile file(pacmanLogPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    // The log grows for years; the last few MiB are enough.
    const qint64 tail = 4 * 1024 * 1024;
    if (file.size() > tail) {
        file.seek(file.size() - tail);
        file.readLine();
    }
    QByteArray started;
    QByteArray last;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine();
        // "[PACMAN] starting full system upgrade"; pamac logs its own tag.
        if (line.contains("] starting full system upgrade")) {
            started = line;
        } else if (!started.isEmpty() && line.contains("] [ALPM] transaction completed")) {
            last = started;
            started.clear();
        }
    }
    if (!last.startsWith('[')) {
        return {};
    }
    // "2026-11-14T10:01:22+0100" -> ISO 8601 with a colon in the offset.
    QString stamp = QString::fromUtf8(last.mid(1, last.indexOf(']') - 1));
    if (stamp.size() > 5 && (stamp.at(stamp.size() - 5) == QLatin1Char('+') || stamp.at(stamp.size() - 5) == QLatin1Char('-'))) {
        stamp.insert(stamp.size() - 2, QLatin1Char(':'));
    }
    return QDateTime::fromString(stamp, Qt::ISODate);
}

QList<NewsItem> parseFeed(const QByteArray &xml, QString *error)
{
    QList<NewsItem> items;
    QXmlStreamReader reader(xml);
    NewsItem current;
    bool inItem = false;
    while (!reader.atEnd()) {
        reader.readNext();
        const auto name = reader.name();
        if (reader.isStartElement()) {
            if (name == QLatin1String("item") || name == QLatin1String("entry")) {
                inItem = true;
                current = {};
            } else if (inItem && name == QLatin1String("title")) {
                current.title = reader.readElementText().simplified();
            } else if (inItem && name == QLatin1String("link")) {
                const QString href = reader.attributes().value(QLatin1String("href")).toString();
                const QString text = reader.readElementText().trimmed();
                if (current.link.isEmpty()) {
                    current.link = href.isEmpty() ? text : href;
                }
            } else if (inItem && (name == QLatin1String("pubDate") || name == QLatin1String("updated") || name == QLatin1String("published"))) {
                const QDateTime dt = parseDate(reader.readElementText());
                if (dt.isValid() && (!current.published.isValid() || name != QLatin1String("updated"))) {
                    current.published = dt;
                }
            }
        } else if (reader.isEndElement() && (name == QLatin1String("item") || name == QLatin1String("entry"))) {
            inItem = false;
            if (!current.title.isEmpty()) {
                items.append(current);
            }
        }
    }
    if (reader.hasError() && error) {
        *error = reader.errorString();
    }
    return items;
}

QString runningKernelPackage(const QString &modulesDir, const QString &release)
{
    const QString running = release.isEmpty() ? QSysInfo::kernelVersion() : release;
    QFile file(modulesDir + QLatin1Char('/') + running + QStringLiteral("/pkgbase"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

NewsResult fetchNews(const NewsSource &source, const QDateTime &since, int timeoutMs)
{
    NewsResult result;
    result.source = source;
    result.since = since.isValid() ? since : QDateTime::currentDateTimeUtc().addDays(-30);
    if (source.url.isEmpty()) {
        result.error = QStringLiteral("no news source");
        return result;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(source.url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("kyu-check"));
    request.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs + 1000, &loop, &QEventLoop::quit);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
    }
    if (reply->error() != QNetworkReply::NoError) {
        result.error = QStringLiteral("%1: %2").arg(source.url, reply->errorString());
        reply->deleteLater();
        return result;
    }
    QString parseError;
    const QList<NewsItem> all = parseFeed(reply->readAll(), &parseError);
    reply->deleteLater();
    if (all.isEmpty() && !parseError.isEmpty()) {
        result.error = QStringLiteral("%1: %2").arg(source.url, parseError);
        return result;
    }
    for (const NewsItem &item : all) {
        if (item.published.isValid() && item.published > result.since) {
            result.items.append(item);
        }
    }
    std::sort(result.items.begin(), result.items.end(), [](const NewsItem &a, const NewsItem &b) {
        return a.published > b.published;
    });
    if (result.items.size() > 10) {
        result.items.resize(10);
    }
    result.ok = true;
    return result;
}
}
