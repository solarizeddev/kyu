// Drives PanelController with the records kyu-helper sends, without D-Bus.

#include "panelcontroller.h"

#include <QJsonArray>
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
        QVERIFY(c.processLine().contains(QStringLiteral("pacman pid 4127")));

        c.handleRecord(rec("stage", {{QStringLiteral("stage"), QStringLiteral("sync")}}));
        QCOMPARE(c.progressText(), QStringLiteral("Synchronizing package databases"));
        c.handleRecord(rec("output", {{QStringLiteral("stream"), QStringLiteral("stdout")}, {QStringLiteral("text"), QStringLiteral("Packages (4) ...")}}));
        c.handleRecord(rec("packages", {{QStringLiteral("total"), 4}}));
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
};

QTEST_GUILESS_MAIN(TestPanel)
#include "test_panel.moc"
