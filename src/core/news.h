#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace Kyu
{
struct NewsItem {
    QString title;
    QString link;
    QDateTime published;

    bool operator==(const NewsItem &) const = default;
};

struct NewsSource {
    QString name; // "Arch Linux news"
    QString url;
};

// Picks the distro's news feed from /etc/os-release (and, on Manjaro, the
// branch in /etc/pacman-mirrors.conf). Arch and Arch-based distros without
// their own feed (ID_LIKE=arch) get Arch's news.
NewsSource newsSourceFor(const QString &osReleasePath = QStringLiteral("/etc/os-release"),
                         const QString &mirrorsConfPath = QStringLiteral("/etc/pacman-mirrors.conf"));

// When the last full system upgrade that completed started, from pacman.log:
// a "[PACMAN] starting full system upgrade" line followed by "[ALPM]
// transaction completed". An upgrade that failed or was stopped does not
// count, so news explaining why stays visible.
QDateTime lastFullUpgrade(const QString &pacmanLogPath = QStringLiteral("/var/log/pacman.log"));

// RSS 2.0 items (Arch, Discourse) or Atom entries.
QList<NewsItem> parseFeed(const QByteArray &xml, QString *error = nullptr);

// Name of the package that installed the running kernel: the pkgbase file in
// its modules directory ("linux618"). Empty if unknown.
QString runningKernelPackage(const QString &modulesDir = QStringLiteral("/usr/lib/modules"), const QString &release = QString());

struct NewsResult {
    bool ok = false;
    QString error;
    NewsSource source;
    QDateTime since;
    QList<NewsItem> items; // newer than `since`, newest first, at most 10
};

// Fetches the feed (http(s) or file URL), keeps items published since the
// last full upgrade (or the last 30 days when pacman.log has none).
NewsResult fetchNews(const NewsSource &source, const QDateTime &since, int timeoutMs = 20000);
}
