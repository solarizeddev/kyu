// Drives PanelController with the records kyu-helper sends, without D-Bus.

#include "helperclient.h"
#include "panelcontroller.h"

#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

using namespace Kyu;

namespace
{
UpdateState sample()
{
    UpdateState s;
    s.checkedAt = QDateTime::currentDateTimeUtc();
    s.pacman = {true, true, {}, s.checkedAt, s.checkedAt};
    s.flatpak = {true, true, {}, s.checkedAt, s.checkedAt};
    // checkupdates prints alphabetically; repos interleave.
    s.packages = {
        {QStringLiteral("glibc"), QStringLiteral("core"), QStringLiteral("2.42-1"), QStringLiteral("2.42-2"), 10 * 1024 * 1024},
        {QStringLiteral("kwin"), QStringLiteral("extra"), QStringLiteral("6.7.3-1"), QStringLiteral("6.7.4-1"), 5 * 1024 * 1024},
        {QStringLiteral("linux618"), QStringLiteral("core"), QStringLiteral("6.18.20-1"), QStringLiteral("6.18.22-1"), 150 * 1024 * 1024},
        {QStringLiteral("mesa"), QStringLiteral("extra"), QStringLiteral("26.2.1-1"), QStringLiteral("26.2.3-1"), -1},
    };
    s.flatpaks = {{QStringLiteral("org.mozilla.firefox"), QStringLiteral("app"), QStringLiteral("stable"), QStringLiteral("flathub"),
                   QStringLiteral("system")}};
    return s;
}

QJsonObject rec(const char *kind, QJsonObject fields = {})
{
    fields.insert(QStringLiteral("kind"), QString::fromLatin1(kind));
    fields.insert(QStringLiteral("tx"), QStringLiteral("20261116T101723Z-00"));
    return fields;
}

QString status(UpdatesModel *m, const QString &name)
{
    for (int i = 0; i < m->rowCount(); ++i) {
        if (m->index(i).data(UpdatesModel::NameRole).toString() == name) {
            return m->index(i).data(UpdatesModel::StatusRole).toString();
        }
    }
    return QStringLiteral("<missing>");
}

QStringList column(UpdatesModel *m, int role)
{
    QStringList values;
    for (int i = 0; i < m->rowCount(); ++i) {
        values.append(m->index(i).data(role).toString());
    }
    return values;
}

void startTransaction(PanelController &c)
{
    c.handleRecord(rec("started",
                       {{QStringLiteral("uid"), 1000},
                        {QStringLiteral("pid"), 4127},
                        {QStringLiteral("time"), QStringLiteral("2026-11-16T10:17:23Z")},
                        {QStringLiteral("command"), QJsonArray{QStringLiteral("/usr/bin/pacman"), QStringLiteral("-Syu")}}}));
}
}

class TestPanel : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void listGroupsByRepoInConfigOrder()
    {
        PanelController c;
        c.setUpdateState(sample());
        auto *m = c.updates();
        QCOMPARE(column(m, UpdatesModel::NameRole),
                 (QStringList{QStringLiteral("glibc"), QStringLiteral("linux618"), QStringLiteral("kwin"), QStringLiteral("mesa"),
                              QStringLiteral("org.mozilla.firefox")}));
        QCOMPARE(m->countInGroup(QStringLiteral("core")), 2);
        QCOMPARE(m->countInGroup(QStringLiteral("flathub")), 1);
        QCOMPARE(c.title(), QStringLiteral("5 updates"));
        QVERIFY2(c.details().startsWith(QStringLiteral("165.0 MiB · checked ")), qPrintable(c.details()));
        QVERIFY(!c.flatpakNote().isEmpty());
        QVERIFY(c.commandPreview().startsWith(QStringLiteral("$ pacman -Syu")));
        QVERIFY(c.commandPreview().contains(QStringLiteral("then Flatpak updates")));
        QCOMPARE(m->index(1).data(UpdatesModel::SizeTextRole).toString(), QStringLiteral("150 MiB"));
        QCOMPARE(m->index(3).data(UpdatesModel::SizeTextRole).toString(), QString()); // mesa: size unknown
        QVERIFY(c.canUpdate());
        QCOMPARE(c.screen(), QStringLiteral("list"));
    }

    void notCheckedYet()
    {
        PanelController c;
        QCOMPARE(c.title(), QStringLiteral("Not checked yet"));
        QVERIFY(!c.canUpdate());
        c.setUpdateState(UpdateState{}, true);
        QCOMPARE(c.title(), QStringLiteral("Checking for updates"));
    }

    void transactionTicksOffTheList()
    {
        PanelController c;
        c.setUpdateState(sample());
        auto *m = c.updates();

        startTransaction(c);
        QCOMPARE(c.screen(), QStringLiteral("running"));
        QVERIFY(!c.canUpdate());
        QCOMPARE(status(m, QStringLiteral("kwin")), QStringLiteral("waiting"));
        QCOMPARE(status(m, QStringLiteral("org.mozilla.firefox")), QStringLiteral("pending"));
        QCOMPARE(c.output()->rowCount(), 1); // "$ /usr/bin/pacman -Syu"
        QCOMPARE(c.output()->tail(5).size(), 1);
        QCOMPARE(c.output()->tail(5)[0].toMap().value(QStringLiteral("line")).toString(), QStringLiteral("$ /usr/bin/pacman -Syu"));
        QVERIFY(c.processLine().contains(QStringLiteral("pacman pid 4127")));

        c.handleRecord(rec("stage", {{QStringLiteral("stage"), QStringLiteral("sync")}}));
        QCOMPARE(c.progressText(), QStringLiteral("Synchronizing package databases"));
        c.handleRecord(rec("output", {{QStringLiteral("stream"), QStringLiteral("stdout")}, {QStringLiteral("text"), QStringLiteral("Packages (4) ...")}}));
        c.handleRecord(rec("packages", {{QStringLiteral("total"), 4}}));
        QVERIFY(c.transactionNote().isEmpty()); // the same 4 as the list
        c.handleRecord(rec("stage", {{QStringLiteral("stage"), QStringLiteral("commit")}}));

        auto progress = [&](const char *action, const char *package, int current) {
            c.handleRecord(rec("progress",
                               {{QStringLiteral("action"), QString::fromLatin1(action)},
                                {QStringLiteral("package"), QString::fromLatin1(package)},
                                {QStringLiteral("current"), current},
                                {QStringLiteral("total"), 4}}));
        };
        progress("upgrading", "kwin", 1);
        QCOMPARE(status(m, QStringLiteral("kwin")), QStringLiteral("working"));
        QCOMPARE(c.percent(), 0); // nothing completed yet
        QCOMPARE(c.progressText(), QStringLiteral("upgrading 1 of 4"));

        progress("installing", "new-dependency", 2);
        QCOMPARE(status(m, QStringLiteral("kwin")), QStringLiteral("done"));
        QCOMPARE(status(m, QStringLiteral("new-dependency")), QStringLiteral("working"));
        QCOMPARE(c.percent(), 25);
        // Added after the pacman rows, before the Flatpaks, in its own group.
        QCOMPARE(column(m, UpdatesModel::NameRole).indexOf(QStringLiteral("new-dependency")), 4);
        QCOMPARE(m->countInGroup(UpdatesModel::extraGroup()), 1);

        progress("upgrading", "linux618", 3);
        progress("upgrading", "glibc", 4);

        // A check finishing mid-transaction must not reset the list.
        UpdateState newer = sample();
        newer.packages.clear();
        c.setUpdateState(newer);
        QCOMPARE(status(m, QStringLiteral("glibc")), QStringLiteral("working"));

        c.handleRecord(rec("finished",
                           {{QStringLiteral("success"), true},
                            {QStringLiteral("exitCode"), 0},
                            {QStringLiteral("summary"), QStringLiteral("3 upgraded, 1 installed")},
                            {QStringLiteral("time"), QStringLiteral("2026-11-16T10:17:30Z")}}));
        QCOMPARE(c.screen(), QStringLiteral("finished"));
        QVERIFY(c.success());
        QCOMPARE(c.resultTitle(), QStringLiteral("3 upgraded, 1 installed"));
        QVERIFY2(c.resultDetails().contains(QStringLiteral("took 00:07 · pacman exit code 0")), qPrintable(c.resultDetails()));
        QCOMPARE(status(m, QStringLiteral("glibc")), QStringLiteral("done"));
        QCOMPARE(status(m, QStringLiteral("mesa")), QStringLiteral("unchanged"));
        QCOMPARE(status(m, QStringLiteral("org.mozilla.firefox")), QStringLiteral("pending"));

        const auto changes = c.changes();
        QCOMPARE(changes.size(), 4);
        QCOMPARE(changes[0].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("kwin"));
        QCOMPARE(changes[1].toMap().value(QStringLiteral("action")).toString(), QStringLiteral("installing"));
        QCOMPARE(changes[2].toMap().value(QStringLiteral("oldVersion")).toString(), QStringLiteral("6.18.20-1"));

        // Done shows the newer check.
        c.done();
        QCOMPARE(c.screen(), QStringLiteral("list"));
        QCOMPARE(column(m, UpdatesModel::NameRole), QStringList{QStringLiteral("org.mozilla.firefox")});
    }

    void failureQuotesPacman()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleRecord(rec("progress",
                           {{QStringLiteral("action"), QStringLiteral("upgrading")},
                            {QStringLiteral("package"), QStringLiteral("kwin")},
                            {QStringLiteral("current"), 1},
                            {QStringLiteral("total"), 4}}));
        const QString summary = QStringLiteral("pacman: failed to commit transaction (conflicting files). 1 upgraded before the error.");
        c.handleRecord(rec("finished",
                           {{QStringLiteral("success"), false}, {QStringLiteral("exitCode"), 1}, {QStringLiteral("summary"), summary}}));
        QCOMPARE(c.resultTitle(), QStringLiteral("Update failed"));
        QCOMPARE(c.resultText(), summary);
        QCOMPARE(status(c.updates(), QStringLiteral("kwin")), QStringLiteral("failed"));
        QVERIFY(c.changes().isEmpty());
    }

    void questionIsShown()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleRecord(rec("question", {{QStringLiteral("text"), QStringLiteral(":: a and b are in conflict. Remove b? [y/N]")}}));
        QCOMPARE(c.question(), QStringLiteral(":: a and b are in conflict. Remove b? [y/N]"));
        c.handleRecord(rec("finished", {{QStringLiteral("success"), false}, {QStringLiteral("summary"), QStringLiteral("Stopped at ...")}}));
        QCOMPARE(c.resultTitle(), QStringLiteral("Update stopped"));
    }

    void providerQuestion()
    {
        PanelController c;
        c.setOwnUid(1000);
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleRecord(rec("question",
                           {{QStringLiteral("id"), 1},
                            {QStringLiteral("handled"), true},
                            {QStringLiteral("type"), QStringLiteral("provider")},
                            {QStringLiteral("text"), QStringLiteral("Enter a number (default=1):")},
                            {QStringLiteral("context"),
                             QJsonArray{QStringLiteral(":: There are 2 providers available for vulkan-driver:"), QStringLiteral(":: Repository extra"),
                                        QStringLiteral("   1) nvidia-utils  2) vulkan-radeon"), QString()}},
                            {QStringLiteral("answers"),
                             QJsonArray{QJsonObject{{QStringLiteral("value"), QStringLiteral("1")},
                                                    {QStringLiteral("label"), QStringLiteral("nvidia-utils")},
                                                    {QStringLiteral("detail"), QStringLiteral("extra")}},
                                        QJsonObject{{QStringLiteral("value"), QStringLiteral("2")},
                                                    {QStringLiteral("label"), QStringLiteral("vulkan-radeon")},
                                                    {QStringLiteral("detail"), QStringLiteral("extra")}}}},
                            {QStringLiteral("default"), QStringLiteral("1")},
                            {QStringLiteral("details"), QJsonObject{{QStringLiteral("dependency"), QStringLiteral("vulkan-driver")}}},
                            {QStringLiteral("timeoutSeconds"), 1800}}));
        QCOMPARE(c.screen(), QStringLiteral("question"));
        QCOMPARE(c.questionTitle(), QStringLiteral("Pick a provider for vulkan-driver"));
        QCOMPARE(c.pausedText(), QStringLiteral("Paused, nothing changed yet"));
        QVERIFY(c.canAnswer());
        const auto options = c.questionOptions();
        QCOMPARE(options.size(), 2);
        QCOMPARE(options[0].toMap().value(QStringLiteral("detail")).toString(), QStringLiteral("extra · pacman's default"));
        QCOMPARE(options[1].toMap().value(QStringLiteral("label")).toString(), QStringLiteral("vulkan-radeon"));
        // pacman's text, verbatim, without blank lines.
        QCOMPARE(c.questionPacman().split(QLatin1Char('\n')).size(), 4);
        QVERIFY(c.questionNote().contains(QStringLiteral("30 minutes")));

        c.handleRecord(rec("answered",
                           {{QStringLiteral("id"), 1},
                            {QStringLiteral("question"), QStringLiteral("Enter a number (default=1):")},
                            {QStringLiteral("answer"), QStringLiteral("2")},
                            {QStringLiteral("uid"), 1000}}));
        QCOMPARE(c.screen(), QStringLiteral("running"));
        QVERIFY(!c.canAnswer());
    }

    void conflictWordingAndOtherUsers()
    {
        PanelController c;
        c.setOwnUid(1001); // not the user who started the update
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleRecord(rec("question",
                           {{QStringLiteral("id"), 1},
                            {QStringLiteral("handled"), true},
                            {QStringLiteral("type"), QStringLiteral("conflict")},
                            {QStringLiteral("text"), QStringLiteral(":: foo and bar are in conflict. Remove bar? [y/N]")},
                            {QStringLiteral("default"), QStringLiteral("n")},
                            {QStringLiteral("details"),
                             QJsonObject{{QStringLiteral("install"), QStringLiteral("foo")}, {QStringLiteral("remove"), QStringLiteral("bar")}}}}));
        QCOMPARE(c.questionTitle(), QStringLiteral("foo conflicts with bar"));
        const auto options = c.questionOptions();
        QCOMPARE(options[0].toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Remove bar"));
        QCOMPARE(options[1].toMap().value(QStringLiteral("detail")).toString(),
                 QStringLiteral("pacman stops the update; nothing is changed · pacman's default"));
        QVERIFY(!c.canAnswer());
        QVERIFY(c.questionNote().startsWith(QStringLiteral("uid 1000 started this update")));

        c.setOwnUid(0);
        QVERIFY(c.canAnswer()); // root may answer
    }

    void skipOffersOnlyCancel()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleRecord(rec("question",
                           {{QStringLiteral("id"), 1},
                            {QStringLiteral("handled"), true},
                            {QStringLiteral("type"), QStringLiteral("skip")},
                            {QStringLiteral("default"), QStringLiteral("n")},
                            {QStringLiteral("details"), QJsonObject{{QStringLiteral("packages"), QStringLiteral("foo bar")}}}}));
        QVERIFY(c.questionOptions().isEmpty());
        QVERIFY(c.questionText().contains(QStringLiteral("partial upgrade")));
        c.handleRecord(rec("cancelled", {{QStringLiteral("id"), 1}, {QStringLiteral("uid"), 1000}}));
        QCOMPARE(c.screen(), QStringLiteral("running"));
        c.handleRecord(rec("finished", {{QStringLiteral("success"), false}, {QStringLiteral("summary"), QStringLiteral("Cancelled at ...")}}));
        QCOMPARE(c.resultTitle(), QStringLiteral("Update cancelled"));
    }

    void pacmanThenFlatpaks()
    {
        PanelController c;
        c.setUpdateState(sample());
        auto *m = c.updates();
        startTransaction(c);
        auto fp = [&c](const char *kind, QJsonObject fields = {}) {
            fields.insert(QStringLiteral("source"), QStringLiteral("flatpak"));
            fields.insert(QStringLiteral("kind"), QString::fromLatin1(kind));
            c.handleFlatpakRecord(fields);
        };
        fp("started", {{QStringLiteral("pid"), 77}, {QStringLiteral("log"), QStringLiteral("/state/kyu/flatpak/x.jsonl")},
                       {QStringLiteral("command"), QJsonArray{QStringLiteral("kyu-flatpak"), QStringLiteral("--after"), QStringLiteral("tx")}}});
        fp("stage", {{QStringLiteral("stage"), QStringLiteral("waiting")}});
        QCOMPARE(status(m, QStringLiteral("org.mozilla.firefox")), QStringLiteral("waiting"));
        c.handleRecord(rec("packages", {{QStringLiteral("total"), 4}}));
        QVERIFY(c.progressText().endsWith(QStringLiteral(" · Flatpaks next")));
        QVERIFY(c.processLine().contains(QStringLiteral("kyu-flatpak pid 77")));
        QCOMPARE(c.logPath(), QStringLiteral("/state/kyu/flatpak/x.jsonl")); // no helper here, so no pacman log

        for (int i = 1; i <= 4; ++i) {
            const char *names[] = {"glibc", "linux618", "kwin", "mesa"};
            c.handleRecord(rec("progress", {{QStringLiteral("action"), QStringLiteral("upgrading")},
                                            {QStringLiteral("package"), QString::fromLatin1(names[i - 1])},
                                            {QStringLiteral("current"), i},
                                            {QStringLiteral("total"), 4}}));
        }
        // 3 of 4 pacman operations done, Flatpak part (1) not started.
        QCOMPARE(c.percent(), 60);
        c.handleRecord(rec("finished", {{QStringLiteral("success"), true}, {QStringLiteral("summary"), QStringLiteral("4 upgraded")}}));
        QCOMPARE(c.screen(), QStringLiteral("running")); // Flatpaks still to come
        QCOMPARE(status(m, QStringLiteral("mesa")), QStringLiteral("done"));
        QCOMPARE(c.percent(), 80);

        fp("stage", {{QStringLiteral("stage"), QStringLiteral("flatpak")}});
        fp("packages", {{QStringLiteral("total"), 1}});
        fp("progress", {{QStringLiteral("action"), QStringLiteral("updating")}, {QStringLiteral("package"), QStringLiteral("org.mozilla.firefox")},
                        {QStringLiteral("current"), 1}, {QStringLiteral("total"), 1}});
        QCOMPARE(c.progressText(), QStringLiteral("updating Flatpak 1 of 1"));
        QCOMPARE(status(m, QStringLiteral("org.mozilla.firefox")), QStringLiteral("working"));
        fp("finished", {{QStringLiteral("success"), true}, {QStringLiteral("changed"), 1}, {QStringLiteral("summary"), QStringLiteral("1 Flatpak updated")}});
        QCOMPARE(c.screen(), QStringLiteral("finished"));
        QVERIFY(c.success());
        QCOMPARE(c.resultTitle(), QStringLiteral("4 upgraded · 1 Flatpak updated"));
        QCOMPARE(status(m, QStringLiteral("org.mozilla.firefox")), QStringLiteral("done"));
        QCOMPARE(c.changes().size(), 5);
    }

    void flatpakOnly()
    {
        UpdateState s = sample();
        s.packages.clear();
        PanelController c;
        c.setUpdateState(s);
        QVERIFY(c.flatpakNote().startsWith(QStringLiteral("Update all updates the Flatpaks")));
        QJsonObject started{{QStringLiteral("kind"), QStringLiteral("started")}, {QStringLiteral("source"), QStringLiteral("flatpak")},
                            {QStringLiteral("pid"), 5}, {QStringLiteral("command"), QJsonArray{QStringLiteral("kyu-flatpak")}}};
        c.handleFlatpakRecord(started);
        QCOMPARE(c.screen(), QStringLiteral("running"));
        QVERIFY(!c.processLine().contains(QStringLiteral("root")));
        c.handleFlatpakRecord({{QStringLiteral("kind"), QStringLiteral("finished")}, {QStringLiteral("success"), true},
                               {QStringLiteral("summary"), QStringLiteral("1 Flatpak updated")}});
        QCOMPARE(c.screen(), QStringLiteral("finished"));
        QCOMPARE(c.resultTitle(), QStringLiteral("1 Flatpak updated"));
    }

    void cancelledPacmanSkipsFlatpaks()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleFlatpakRecord({{QStringLiteral("kind"), QStringLiteral("started")}, {QStringLiteral("pid"), 5}});
        c.handleRecord(rec("question", {{QStringLiteral("id"), 1}, {QStringLiteral("handled"), true}, {QStringLiteral("type"), QStringLiteral("conflict")}}));
        c.handleRecord(rec("cancelled", {{QStringLiteral("id"), 1}, {QStringLiteral("uid"), 1000}}));
        c.handleRecord(rec("finished", {{QStringLiteral("success"), false}, {QStringLiteral("summary"), QStringLiteral("Cancelled at ...")}}));
        QCOMPARE(c.screen(), QStringLiteral("running"));
        c.handleFlatpakRecord({{QStringLiteral("kind"), QStringLiteral("finished")}, {QStringLiteral("success"), false},
                               {QStringLiteral("skipped"), true},
                               {QStringLiteral("summary"), QStringLiteral("Flatpaks not updated: the pacman update was stopped at a question")}});
        QCOMPARE(c.screen(), QStringLiteral("finished"));
        QCOMPARE(c.resultTitle(), QStringLiteral("Update cancelled"));
        QVERIFY(c.resultText().contains(QStringLiteral("Flatpaks not updated")));
        QCOMPARE(status(c.updates(), QStringLiteral("org.mozilla.firefox")), QStringLiteral("unchanged"));
    }

    void flatpakFailureAfterPacman()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleFlatpakRecord({{QStringLiteral("kind"), QStringLiteral("started")}, {QStringLiteral("pid"), 5}});
        c.handleRecord(rec("finished", {{QStringLiteral("success"), true}, {QStringLiteral("summary"), QStringLiteral("4 upgraded")}}));
        c.handleFlatpakLost(QStringLiteral("/state/kyu/flatpak/x.jsonl"));
        QCOMPARE(c.screen(), QStringLiteral("finished"));
        QCOMPARE(c.resultTitle(), QStringLiteral("Update failed"));
        QCOMPARE(c.resultText().split(QLatin1Char('\n')).first(), QStringLiteral("4 upgraded"));
        QVERIFY(c.resultText().contains(QStringLiteral("kyu-flatpak stopped before it finished")));
    }

    void listShowsRestartAndNews()
    {
        UpdateState s = sample();
        s.packages[2].restart = true; // linux618
        s.news.checked = true;
        s.news.ok = true;
        s.news.source = QStringLiteral("Arch Linux news");
        s.news.since = QDateTime(QDate(2026, 11, 10), QTime(12, 0), Qt::UTC);
        s.news.items = {{QStringLiteral("Manual intervention for foo"), QStringLiteral("https://archlinux.org/news/foo/"),
                         QDateTime(QDate(2026, 11, 14), QTime(12, 0), Qt::UTC)}};
        PanelController c;
        c.setUpdateState(s);
        QVERIFY(c.details().contains(QStringLiteral("restart needed")));
        QCOMPARE(c.updates()->index(1).data(UpdatesModel::NameRole).toString(), QStringLiteral("linux618"));
        QVERIFY(c.updates()->index(1).data(UpdatesModel::RestartRole).toBool());
        QVERIFY(!c.updates()->index(0).data(UpdatesModel::RestartRole).toBool());
        QCOMPARE(c.newsText(), QStringLiteral("Arch Linux news since your last upgrade: 1 · Manual intervention for foo"));
        QCOMPARE(c.newsUrl(), QStringLiteral("https://archlinux.org/news/foo/")); // one item: that post
        QCOMPARE(c.newsSource(), QStringLiteral("Arch Linux news · 1 since your last upgrade"));
        QCOMPARE(c.newsHeadline(), QStringLiteral("Manual intervention for foo"));

        // Only before a system upgrade: not with Flatpak updates alone.
        s.packages.clear();
        c.setUpdateState(s);
        QVERIFY(c.newsText().isEmpty());

        // Manjaro: the branch's update announcement.
        s = sample();
        s.news = {true, true, {}, {}, QStringLiteral("Manjaro stable updates"),
                  QStringLiteral("https://forum.manjaro.org/c/announcements/stable-updates/12.rss"),
                  QStringLiteral("https://forum.manjaro.org/c/announcements/stable-updates/12"), {},
                  {{QStringLiteral("[Stable Update] 2026-11-12 - Kernels, Plasma"), QStringLiteral("https://forum.manjaro.org/t/stable-update/190001"),
                    QDateTime(QDate(2026, 11, 12), QTime(9, 0), Qt::UTC)}}};
        c.setUpdateState(s);
        QCOMPARE(c.newsSource(), QStringLiteral("Manjaro stable updates · 1 since your last upgrade"));
        QCOMPARE(c.newsHeadline(), QStringLiteral("[Stable Update] 2026-11-12 - Kernels, Plasma"));
        QCOMPARE(c.newsUrl(), QStringLiteral("https://forum.manjaro.org/t/stable-update/190001"));

        // Nothing new: no line at all.
        s = sample();
        s.news = {true, true, {}, {}, QStringLiteral("Arch Linux news"), QStringLiteral("https://archlinux.org/feeds/news/"),
                  QStringLiteral("https://archlinux.org/news/"), {}, {}};
        c.setUpdateState(s);
        QVERIFY(c.newsText().isEmpty());
        QVERIFY(c.newsError().isEmpty());

        // A failed fetch is shown as such.
        s = sample();
        s.news = {true, false, QStringLiteral("https://archlinux.org/feeds/news/: Host not found"), {}, QStringLiteral("Arch Linux news"),
                  QStringLiteral("https://archlinux.org/feeds/news/"), QStringLiteral("https://archlinux.org/news/"), {}, {}};
        c.setUpdateState(s);
        QCOMPARE(c.newsError(), QStringLiteral("Arch Linux news not checked: https://archlinux.org/feeds/news/: Host not found"));
    }

    void finishedWithRestartAndSettingsFiles()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleRecord(rec("finished",
                           {{QStringLiteral("success"), true},
                            {QStringLiteral("summary"), QStringLiteral("4 upgraded")},
                            {QStringLiteral("configs"),
                             QJsonArray{QJsonObject{{QStringLiteral("kind"), QStringLiteral("pacnew")},
                                                    {QStringLiteral("path"), QStringLiteral("/etc/pacman.conf")},
                                                    {QStringLiteral("saved"), QStringLiteral("/etc/pacman.conf.pacnew")}}}},
                            {QStringLiteral("restart"),
                             QJsonObject{{QStringLiteral("required"), true},
                                         {QStringLiteral("systemd"), false},
                                         {QStringLiteral("kernel"),
                                          QJsonObject{{QStringLiteral("running"), QStringLiteral("6.18.20-1")},
                                                      {QStringLiteral("installed"), QStringLiteral("6.18.22-1")},
                                                      {QStringLiteral("replaced"), true}}},
                                         {QStringLiteral("processes"),
                                          QJsonArray{QJsonObject{{QStringLiteral("pid"), 42},
                                                                 {QStringLiteral("name"), QStringLiteral("plasmashell")},
                                                                 {QStringLiteral("libraries"), QJsonArray{QStringLiteral("/usr/lib/libQt6Core.so.6")}}}}},
                                         {QStringLiteral("processCount"), 1}}}}));
        QCOMPARE(c.screen(), QStringLiteral("finished"));
        QCOMPARE(c.resultTitle(), QStringLiteral("4 upgraded, restart required"));
        QVERIFY2(c.resultDetails().startsWith(QStringLiteral("running 6.18.20-1 · installed 6.18.22-1")), qPrintable(c.resultDetails()));
        QVERIFY(c.restartReason().contains(QStringLiteral("kernel was replaced")));
        QCOMPARE(c.processCount(), 1);
        QCOMPARE(c.processes()[0].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("plasmashell"));
        QCOMPARE(c.configs().size(), 1);
        QCOMPARE(c.configs()[0].toMap().value(QStringLiteral("saved")).toString(), QStringLiteral("/etc/pacman.conf.pacnew"));
    }

    void diffOfSettingsFiles()
    {
        QTemporaryDir dir;
        auto write = [&dir](const char *name, const QByteArray &content) {
            QFile f(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(content);
        };
        write("a.conf", "keep\nold value\n");
        write("a.conf.pacnew", "keep\nnew value\n");
        write("same.conf", "x\n");
        write("same.conf.pacnew", "x\n");
        PanelController c;
        const auto lines = c.diff(dir.filePath(QStringLiteral("a.conf")), dir.filePath(QStringLiteral("a.conf.pacnew")));
        QStringList kinds;
        for (const auto &l : lines) {
            kinds.append(l.toMap().value(QStringLiteral("kind")).toString());
        }
        QVERIFY(kinds.contains(QStringLiteral("remove")));
        QVERIFY(kinds.contains(QStringLiteral("add")));
        QVERIFY(kinds.contains(QStringLiteral("hunk")));
        const auto same = c.diff(dir.filePath(QStringLiteral("same.conf")), dir.filePath(QStringLiteral("same.conf.pacnew")));
        QCOMPARE(same[0].toMap().value(QStringLiteral("text")).toString(), QStringLiteral("The two files are the same."));
        const auto missing = c.diff(dir.filePath(QStringLiteral("gone.conf")), dir.filePath(QStringLiteral("a.conf.pacnew")));
        QCOMPARE(missing[0].toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("error"));
    }

    void failureIsExplained()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleRecord(rec("finished",
                           {{QStringLiteral("success"), false},
                            {QStringLiteral("summary"), QStringLiteral("pacman: failed to commit transaction (conflicting files). No packages were changed.")},
                            {QStringLiteral("failure"),
                             QJsonObject{{QStringLiteral("kind"), QStringLiteral("files")},
                                         {QStringLiteral("files"), QJsonArray{QStringLiteral("mesa: /usr/lib/libx.so")}},
                                         {QStringLiteral("fileCount"), 1},
                                         {QStringLiteral("lines"), QJsonArray{QStringLiteral("error: failed to commit transaction (conflicting files)")}}}}}));
        QCOMPARE(c.resultTitle(), QStringLiteral("Update failed"));
        QCOMPARE(c.failureTitle(), QStringLiteral("Files already exist"));
        QVERIFY(c.failureText().startsWith(QStringLiteral("One file the update would install is already on disk")));
        QCOMPARE(c.failureItems(), QStringList{QStringLiteral("mesa: /usr/lib/libx.so")});
        QCOMPARE(c.failureLines().size(), 1);

        // A new run starts with no failure.
        startTransaction(c);
        QVERIFY(c.failureTitle().isEmpty());
    }

    void interruptedUpdateAndStaleLock()
    {
        PanelController c;
        c.setUpdateState(sample());
        c.setHelperState({{QStringLiteral("phase"), QStringLiteral("idle")},
                          {QStringLiteral("previous"),
                           QJsonObject{{QStringLiteral("tx"), QStringLiteral("20261116T101723Z-00")},
                                       {QStringLiteral("log"), QStringLiteral("/var/log/kyu/20261116T101723Z-00.jsonl")},
                                       {QStringLiteral("interrupted"), true}}},
                          {QStringLiteral("lock"),
                           QJsonObject{{QStringLiteral("path"), QStringLiteral("/var/lib/pacman/db.lck")},
                                       {QStringLiteral("exists"), true},
                                       {QStringLiteral("holder"), QJsonValue::Null}}}});
        QVERIFY(c.interruptedText().contains(QStringLiteral("did not finish")));
        QVERIFY(c.canRemoveLock());

        // Held by another process: not offered.
        c.setHelperState({{QStringLiteral("lock"),
                           QJsonObject{{QStringLiteral("exists"), true},
                                       {QStringLiteral("holder"), QJsonObject{{QStringLiteral("pid"), 77}, {QStringLiteral("name"), QStringLiteral("pamac-daemon")}}}}}});
        QVERIFY(!c.canRemoveLock());
        QVERIFY(c.interruptedText().isEmpty());
    }

    void startErrorsAreQuoted()
    {
        PanelController c;
        c.setUpdateState(sample());
        c.handleStartFinished({}, QStringLiteral("io.github.solarizeddev.kyu.Helper1.Error.Locked"),
                              QStringLiteral("/var/lib/pacman/db.lck exists."));
        QCOMPARE(c.message(), QStringLiteral("pacman's database is locked: /var/lib/pacman/db.lck exists."));
        QCOMPARE(c.screen(), QStringLiteral("list"));
        QVERIFY(c.canUpdate());
    }

    void helperLost()
    {
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        c.handleHelperLost(QStringLiteral("/var/log/kyu/x.jsonl"));
        QCOMPARE(c.screen(), QStringLiteral("finished"));
        QVERIFY(!c.success());
        QVERIFY(c.resultText().contains(QStringLiteral("/var/log/kyu/x.jsonl")));
    }

    void transactionDiffersFromTheList()
    {
        // The mirror changed after the check: pacman upgrades 6, not 4.
        PanelController c;
        c.setUpdateState(sample());
        startTransaction(c);
        QVERIFY(c.transactionNote().isEmpty()); // not known yet
        c.handleRecord(rec("packages", {{QStringLiteral("total"), 6}}));
        const QString note = QStringLiteral("pacman's transaction has 6 packages; the list showed 4 (new dependencies, or the mirrors "
                                            "changed since the last check). pacman's own list is in the output.");
        QCOMPARE(c.transactionNote(), note);
        // A check that finishes meanwhile does not change what was shown.
        UpdateState after = sample();
        after.packages.clear();
        c.setUpdateState(after);
        QCOMPARE(c.transactionNote(), note);
    }

    void testModeNeverRestarts()
    {
        // With a session-bus test helper "Restart now" must not reach logind.
        HelperClient helper(HelperClient::Bus::Session);
        PanelController c;
        c.setHelper(&helper, true);
        c.restartNow();
        QVERIFY(c.message().contains(QStringLiteral("not restarted")));
    }
};

QTEST_GUILESS_MAIN(TestPanel)
#include "test_panel.moc"
