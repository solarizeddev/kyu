// Fixtures are plain string literals: moc in Qt 6.4 cannot parse raw strings.
#include "news.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Kyu;

namespace
{
QString write(const QTemporaryDir &dir, const char *name, const QByteArray &content)
{
    const QString path = dir.filePath(QString::fromLatin1(name));
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(content);
    }
    return path;
}
}

class TestNews : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void rss()
    {
        const QByteArray xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<rss version=\"2.0\"><channel><title>Arch Linux: Recent news updates</title>\n"
            "<item><title>Manual intervention for foo</title><link>https://archlinux.org/news/foo/</link>\n"
            "<pubDate>Sat, 14 Nov 2026 10:00:00 +0000</pubDate></item>\n"
            "<item><title>Older &amp; done</title><link>https://archlinux.org/news/old/</link>\n"
            "<pubDate>Mon, 01 Jun 2026 08:00:00 +0000</pubDate></item>\n"
            "</channel></rss>";
        const auto items = parseFeed(xml);
        QCOMPARE(items.size(), 2);
        QCOMPARE(items[0].title, QStringLiteral("Manual intervention for foo"));
        QCOMPARE(items[0].link, QStringLiteral("https://archlinux.org/news/foo/"));
        QCOMPARE(items[0].published, QDateTime(QDate(2026, 11, 14), QTime(10, 0), Qt::UTC));
        QCOMPARE(items[1].title, QStringLiteral("Older & done"));
    }

    void atom()
    {
        const QByteArray xml = "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
            "<entry><title>Stable update 2026-11-15</title><link href=\"https://example.org/t/1\"/><published>2026-11-15T09:00:00Z</published>\n"
            "<updated>2026-11-16T09:00:00Z</updated></entry></feed>";
        const auto items = parseFeed(xml);
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].link, QStringLiteral("https://example.org/t/1"));
        QCOMPARE(items[0].published.date(), QDate(2026, 11, 15)); // published, not updated
    }

    void sourceByDistro()
    {
        QTemporaryDir dir;
        QCOMPARE(newsSourceFor(write(dir, "arch", "NAME=\"Arch Linux\"\nID=arch\n")).url, QStringLiteral("https://archlinux.org/feeds/news/"));
        QCOMPARE(newsSourceFor(write(dir, "eos", "ID=endeavouros\nID_LIKE=arch\n")).name, QStringLiteral("Arch Linux news"));
        QCOMPARE(newsSourceFor(write(dir, "cachy", "ID=cachyos\nID_LIKE=\"arch\"\n")).name, QStringLiteral("Arch Linux news"));
        const QString manjaro = write(dir, "manjaro", "ID=manjaro\nID_LIKE=arch\n");
        // Manjaro: a link to the branch's announcements, no feed to fetch.
        QVERIFY(newsSourceFor(manjaro, dir.filePath(QStringLiteral("none"))).url.isEmpty());
        QCOMPARE(newsSourceFor(manjaro, dir.filePath(QStringLiteral("none"))).page,
                 QStringLiteral("https://forum.manjaro.org/c/announcements/stable-updates"));
        QCOMPARE(newsSourceFor(manjaro, write(dir, "mirrors", "# comment\nBranch = testing\n")).page,
                 QStringLiteral("https://forum.manjaro.org/c/announcements/testing-updates"));
        const auto linkOnly = fetchNews(newsSourceFor(manjaro, dir.filePath(QStringLiteral("none"))), {});
        QVERIFY(linkOnly.ok);
        QVERIFY(linkOnly.items.isEmpty());
        QVERIFY(newsSourceFor(write(dir, "ubuntu", "ID=ubuntu\nID_LIKE=debian\n")).url.isEmpty());
    }

    void lastUpgradeFromPacmanLog()
    {
        QTemporaryDir dir;
        const QString log = write(dir, "pacman.log",
                                  "[2026-11-01T09:00:00+0100] [PACMAN] Running 'pacman -Syu'\n"
                                  "[2026-11-01T09:00:01+0100] [PACMAN] starting full system upgrade\n"
                                  "[2026-11-01T09:00:30+0100] [ALPM] transaction completed\n"
                                  "[2026-11-10T20:30:00+0100] [PACMAN] starting full system upgrade\n"
                                  "[2026-11-10T20:31:00+0100] [ALPM] transaction completed\n"
                                  "[2026-11-12T08:00:00+0100] [PACMAN] Running 'pacman -S foo'\n"
                                  // A later upgrade that failed: not counted.
                                  "[2026-11-15T08:00:00+0100] [PACMAN] starting full system upgrade\n"
                                  "[2026-11-15T08:00:05+0100] [PACMAN] Running 'pacman -Syu'\n");
        QCOMPARE(lastFullUpgrade(log), QDateTime(QDate(2026, 11, 10), QTime(19, 30), Qt::UTC));
        QVERIFY(!lastFullUpgrade(dir.filePath(QStringLiteral("missing"))).isValid());
    }

    void runningKernel()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath(QStringLiteral("6.18.20-1-MANJARO")));
        write(dir, "6.18.20-1-MANJARO/pkgbase", "linux618\n");
        QCOMPARE(runningKernelPackage(dir.path(), QStringLiteral("6.18.20-1-MANJARO")), QStringLiteral("linux618"));
        QVERIFY(runningKernelPackage(dir.path(), QStringLiteral("6.12.1-1")).isEmpty());
    }

    void fetchKeepsItemsSinceTheLastUpgrade()
    {
        QTemporaryDir dir;
        const QString feed = write(dir, "feed.xml", "<rss><channel>\n"
            "<item><title>A</title><link>a</link><pubDate>Sat, 14 Nov 2026 10:00:00 +0000</pubDate></item>\n"
            "<item><title>B</title><link>b</link><pubDate>Thu, 12 Nov 2026 10:00:00 +0000</pubDate></item>\n"
            "<item><title>C</title><link>c</link><pubDate>Mon, 09 Nov 2026 10:00:00 +0000</pubDate></item>\n"
            "</channel></rss>");
        const auto r = fetchNews({QStringLiteral("Test"), QUrl::fromLocalFile(feed).toString()}, QDateTime(QDate(2026, 11, 10), QTime(0, 0), Qt::UTC));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.items.size(), 2);
        QCOMPARE(r.items[0].title, QStringLiteral("A"));

        const auto missing = fetchNews({QStringLiteral("Test"), QUrl::fromLocalFile(dir.filePath(QStringLiteral("nope.xml"))).toString()}, {});
        QVERIFY(!missing.ok);
        QVERIFY(!missing.error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestNews)
#include "test_news.moc"
