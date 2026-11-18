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
    QString url; // feed to fetch
    QString page; // the web page people read
};

// Picks the distro's news from /etc/os-release. Arch and Arch-based distros
// that use Arch's packages (ID_LIKE=arch: CachyOS, EndeavourOS) get Arch's
// news feed, the "manual intervention" channel. Manjaro gets its branch's
// update announcements (branch from /etc/pacman-mirrors.conf; the ARM
// categories on arm64): one post per update batch, with what changed and the
// known issues. Arch's news is mistimed for Manjaro's delayed branches. Other
// distributions: none (empty name).
NewsSource newsSourceFor(const QString &osReleasePath = QStringLiteral("/etc/os-release"),
                         const QString &mirrorsConfPath = QStringLiteral("/etc/pacman-mirrors.conf"),
                         const QString &cpuArchitecture = QString());

// A source's name as the state file has it (in English: "Arch Linux news")
// in the user's language. Manjaro's category names stay as they are.
QString newsSourceText(const QString &name);

// When the last full system upgrade that completed started, from pacman.log:
// a "[PACMAN] starting full system upgrade" line (or another frontend's tag) followed by "[ALPM]
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

// Announcements already seen: confirmed in kyu-update ("Continue? y") or
// opened from the card. Kept by link (title when there is none), in
// news-seen.json in Kyu's state directory unless a path is given.
QString newsKey(const NewsItem &item);
QStringList seenNews(const QString &path = QString());
void markNewsSeen(const QStringList &keys, const QString &path = QString());
}
