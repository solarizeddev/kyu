// The planner: what pacman will be asked to do, from pacman's own dry runs
// (answers canned here; the real pacman in tools/docker/scenarios).

#include "plan.h"

#include <QJsonArray>
#include <QTest>

using namespace Kyu;

class FakeEnv : public PlanEnvironment
{
public:
    // "program arg arg" → result; unknown commands fail.
    QHash<QString, CommandResult> answers;
    QStringList ran;
    QSet<QString> files;

    void answer(const QString &command, const QByteArray &out, int code = 0, const QByteArray &err = {})
    {
        answers.insert(command, CommandResult{true, code, out, err});
    }
    CommandResult run(const QString &program, const QStringList &arguments, const QMap<QString, QString> & = {}) override
    {
        const QString command = (QStringList{program} + arguments).join(QLatin1Char(' '));
        ran.append(command);
        return answers.value(command, CommandResult{true, 1, {}, "error: unexpected in test\n"});
    }
    bool isFile(const QString &path) override
    {
        return files.contains(path);
    }
};

class TestPlan : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void parsing();
    void plainInstall();
    void updateFirst_data();
    void updateFirst();
    void virtualTarget();
    void alreadyInstalled();
    void removal();
    void removalBlocked();
    void reinstall();
    void reasons();
    void queueJson();
};

static const QString P = QStringLiteral("--print --print-format %n %v");

// A system with calligra, krita and libwmf installed, nothing pending.
static void baseSystem(FakeEnv &env, int pending = 0)
{
    env.answer(QStringLiteral("pacman -Q"), "calligra 25.08.3-1\nkrita 5.2.13-3\nlibwmf 0.2.13-5\nlibfoo 1.0-1\n");
    env.answer(QStringLiteral("pacman -Qqe"), "calligra\nkrita\n");
    env.answer(QStringLiteral("pacman-conf IgnorePkg"), "");
    env.answer(QStringLiteral("pacman-conf HoldPkg"), "pacman\nglibc\n");
    env.answer(QStringLiteral("checkupdates --nocolor"), pending ? QByteArray("mesa 1:26.2.1-1 -> 1:26.2.3-1\n").repeated(pending) : QByteArray(), pending ? 0 : 2);
}

static QueueItem item(const char *id, QueueItem::Kind kind, const char *name, bool dep = false)
{
    QueueItem i;
    i.id = QString::fromLatin1(id);
    i.kind = kind;
    i.name = QString::fromLatin1(name);
    i.asDependency = dep;
    return i;
}

static PlanOptions opts()
{
    PlanOptions o;
    o.planDb = QStringLiteral("/plan");
    return o;
}

void TestPlan::parsing()
{
    QCOMPARE(parseNameVersion("gimp 3.0.6-2\nwarning: something\nmesa 1:26.2.3-1\n"),
             (QList<std::pair<QString, QString>>{{QStringLiteral("gimp"), QStringLiteral("3.0.6-2")}, {QStringLiteral("mesa"), QStringLiteral("1:26.2.3-1")}}));
    QCOMPARE(dependantsOf(":: removing glibc breaks dependency 'glibc' required by acl\n:: removing glibc breaks dependency 'glibc' required by argon2\n"),
             (QStringList{QStringLiteral("acl"), QStringLiteral("argon2")}));
    QCOMPARE(countUpdates("mesa 1:26.2.1-1 -> 1:26.2.3-1\nkwin 6.7.4-1 -> 6.7.5-1\n"), 2);
}

void TestPlan::plainInstall()
{
    FakeEnv env;
    baseSystem(env);
    env.answer(QStringLiteral("pacman -S ") + P + QStringLiteral(" gimp ghostscript"), "babl 0.1.116-1\ngegl 0.4.62-1\ngimp 3.0.6-2\nghostscript 10.06.0-1\n");
    env.answer(QStringLiteral("pacman --dbpath /plan -S ") + P + QStringLiteral(" gimp ghostscript"), "babl 0.1.116-1\ngegl 0.4.62-1\ngimp 3.0.6-2\nghostscript 10.06.0-1\n");
    env.answer(QStringLiteral("pacman -Rs ") + P + QStringLiteral(" calligra"), "calligra 25.08.3-1\nlibetonyek 0.1.10-1\n");
    Queue q{{item("1", QueueItem::Kind::Remove, "calligra"), item("2", QueueItem::Kind::Install, "gimp"), item("3", QueueItem::Kind::Install, "ghostscript", true)}};
    const Plan plan = makePlan(q, env, opts());
    QVERIFY(!plan.updateFirst);
    QVERIFY(plan.runnable());
    QCOMPARE(plan.steps.size(), 3);
    // Installs first, in one transaction; removals after; reasons last.
    QCOMPARE(plan.steps[0].arguments, (QStringList{QStringLiteral("-S"), QStringLiteral("--needed"), QStringLiteral("gimp"), QStringLiteral("ghostscript")}));
    QCOMPARE(plan.steps[0].packages.size(), 4);
    QCOMPARE(plan.steps[0].packages[2].change, PlannedPackage::Change::New);
    QCOMPARE(plan.steps[1].arguments, (QStringList{QStringLiteral("-Rs"), QStringLiteral("calligra")}));
    QCOMPARE(plan.steps[2].arguments, (QStringList{QStringLiteral("-D"), QStringLiteral("--asdeps"), QStringLiteral("ghostscript")}));
    QCOMPARE(plan.steps[2].items, QStringList{QStringLiteral("3")});
    QCOMPARE(Plan::stepTitle(plan.steps[0]), QStringLiteral("Install gimp, ghostscript"));
}

void TestPlan::updateFirst_data()
{
    QTest::addColumn<int>("pending");
    QTest::addColumn<QByteArray>("system");
    QTest::addColumn<QByteArray>("fresh");
    QTest::addColumn<bool>("first");
    QTest::newRow("current") << 0 << QByteArray("gimp 3.0.6-2\n") << QByteArray("gimp 3.0.6-2\n") << false;
    // Synced (by pamac) but not upgraded: the copies agree, yet updates are pending.
    QTest::newRow("synced, not upgraded") << 3 << QByteArray("gimp 3.0.6-2\n") << QByteArray("gimp 3.0.6-2\n") << true;
    QTest::newRow("newer in the fresh copy") << 0 << QByteArray("gimp 3.0.6-1\n") << QByteArray("gimp 3.0.6-2\n") << true;
    // pacman would upgrade an installed library with it: a partial upgrade.
    QTest::newRow("upgrade inside") << 0 << QByteArray("libfoo 1.1-1\ngimp 3.0.6-2\n") << QByteArray("libfoo 1.1-1\ngimp 3.0.6-2\n") << true;
}

void TestPlan::updateFirst()
{
    QFETCH(int, pending);
    QFETCH(QByteArray, system);
    QFETCH(QByteArray, fresh);
    QFETCH(bool, first);
    FakeEnv env;
    baseSystem(env, pending);
    env.answer(QStringLiteral("pacman -S ") + P + QStringLiteral(" gimp"), system);
    env.answer(QStringLiteral("pacman --dbpath /plan -S ") + P + QStringLiteral(" gimp"), fresh);
    const Plan plan = makePlan(Queue{{item("1", QueueItem::Kind::Install, "gimp")}}, env, opts());
    QCOMPARE(plan.updateFirst, first);
    QCOMPARE(plan.pending, pending);
}

void TestPlan::virtualTarget()
{
    FakeEnv env;
    baseSystem(env);
    // pacman silently takes the default provider: the target's own name is not printed.
    env.answer(QStringLiteral("pacman -S ") + P + QStringLiteral(" java-runtime"), "java-runtime-common 3-6\njre-openjdk 25-1\n");
    env.answer(QStringLiteral("pacman --dbpath /plan -S ") + P + QStringLiteral(" java-runtime"), "java-runtime-common 3-6\njre-openjdk 25-1\n");
    const Plan plan = makePlan(Queue{{item("1", QueueItem::Kind::Install, "java-runtime")}}, env, opts());
    QVERIFY(!plan.runnable());
    QCOMPARE(plan.steps[0].blockedBy, QStringList{QStringLiteral("java-runtime")});
}

void TestPlan::alreadyInstalled()
{
    FakeEnv env;
    baseSystem(env);
    // An optional dependency already installed explicitly: nothing to do, its reason stays.
    const Plan plan = makePlan(Queue{{item("1", QueueItem::Kind::Install, "krita", true)}}, env, opts());
    QVERIFY(plan.steps.isEmpty());
    QCOMPARE(plan.dropped.size(), 1);
    QVERIFY(!plan.runnable());
}

void TestPlan::removal()
{
    FakeEnv env;
    baseSystem(env);
    env.answer(QStringLiteral("pacman -Rs ") + P + QStringLiteral(" krita"), "krita 5.2.13-3\n");
    QueueItem withSettings = item("1", QueueItem::Kind::Remove, "krita");
    withSettings.withSettings = true;
    const Plan plan = makePlan(Queue{{withSettings}}, env, opts());
    QVERIFY(plan.runnable());
    QCOMPARE(plan.steps[0].arguments, (QStringList{QStringLiteral("-Rns"), QStringLiteral("krita")}));
    // Removing needs no fresh lists: no checkupdates.
    QVERIFY(!env.ran.contains(QStringLiteral("checkupdates --nocolor")));
}

void TestPlan::removalBlocked()
{
    FakeEnv env;
    baseSystem(env);
    env.answer(QStringLiteral("pacman -Rs ") + P + QStringLiteral(" libwmf"), "", 1,
               "error: failed to prepare transaction (could not satisfy dependencies)\n:: removing libwmf breaks dependency 'libwmf' required by krita\n");
    Plan plan = makePlan(Queue{{item("1", QueueItem::Kind::Remove, "libwmf")}}, env, opts());
    QVERIFY(!plan.runnable());
    QCOMPARE(plan.steps[0].blockedBy, QStringList{QStringLiteral("krita")});
    QCOMPARE(plan.steps[0].blocked, QStringLiteral("Other installed packages need it: krita."));
    // HoldPkg: never.
    FakeEnv env2;
    baseSystem(env2);
    env2.answer(QStringLiteral("pacman -Q"), "pacman 7.1.0-1\n");
    env2.answer(QStringLiteral("pacman -Rs ") + P + QStringLiteral(" pacman"), "pacman 7.1.0-1\n");
    plan = makePlan(Queue{{item("1", QueueItem::Kind::Remove, "pacman")}}, env2, opts());
    QVERIFY(!plan.runnable());
    QCOMPARE(plan.steps[0].blockedBy, QStringList{QStringLiteral("pacman")});
}

void TestPlan::reinstall()
{
    FakeEnv env;
    baseSystem(env);
    env.answer(QStringLiteral("pacman -S ") + P + QStringLiteral(" krita"), "krita 5.2.13-3\n");
    env.answer(QStringLiteral("pacman --dbpath /plan -S ") + P + QStringLiteral(" krita"), "krita 5.2.13-3\n");
    Plan plan = makePlan(Queue{{item("1", QueueItem::Kind::Reinstall, "krita")}}, env, opts());
    QVERIFY(plan.runnable());
    QCOMPARE(plan.steps[0].arguments, (QStringList{QStringLiteral("-S"), QStringLiteral("krita")}));
    QVERIFY(!plan.updateFirst);
    // A newer version: the update installs it; the item goes.
    env.answer(QStringLiteral("pacman --dbpath /plan -S ") + P + QStringLiteral(" krita"), "krita 5.2.14-1\n");
    plan = makePlan(Queue{{item("1", QueueItem::Kind::Reinstall, "krita")}}, env, opts());
    QVERIFY(plan.steps.isEmpty());
    QCOMPARE(plan.dropped.size(), 1);
}

void TestPlan::reasons()
{
    FakeEnv env;
    baseSystem(env);
    QueueItem toDep = item("1", QueueItem::Kind::Reason, "krita");
    toDep.explicitReason = false;
    QueueItem already = item("2", QueueItem::Kind::Reason, "calligra"); // explicit already
    QueueItem toExplicit = item("3", QueueItem::Kind::Reason, "libwmf");
    const Plan plan = makePlan(Queue{{toDep, already, toExplicit}}, env, opts());
    QCOMPARE(plan.steps.size(), 2);
    QCOMPARE(plan.steps[0].arguments, (QStringList{QStringLiteral("-D"), QStringLiteral("--asdeps"), QStringLiteral("krita")}));
    QCOMPARE(plan.steps[1].arguments, (QStringList{QStringLiteral("-D"), QStringLiteral("--asexplicit"), QStringLiteral("libwmf")}));
    QCOMPARE(plan.dropped.size(), 1);
}

void TestPlan::queueJson()
{
    Queue q{{item("1", QueueItem::Kind::Install, "gimp", true), item("2", QueueItem::Kind::Remove, "calligra")}};
    QString error;
    const auto back = Queue::fromJson(q.toJson(), &error);
    QVERIFY2(back, qPrintable(error));
    QCOMPARE(back->items.size(), 2);
    QVERIFY(back->items[0].asDependency);
    // Nothing that is not a package name reaches a command.
    QJsonObject bad = q.toJson();
    QJsonArray items = bad.value(QStringLiteral("items")).toArray();
    QJsonObject first = items[0].toObject();
    first.insert(QStringLiteral("name"), QStringLiteral("gimp; rm -rf /"));
    items[0] = first;
    bad.insert(QStringLiteral("items"), items);
    QVERIFY(!Queue::fromJson(bad));
    QJsonObject flag = q.toJson();
    items = flag.value(QStringLiteral("items")).toArray();
    first = items[0].toObject();
    first.insert(QStringLiteral("name"), QStringLiteral("--noconfirm"));
    items[0] = first;
    flag.insert(QStringLiteral("items"), items);
    QVERIFY(!Queue::fromJson(flag));
}

QTEST_GUILESS_MAIN(TestPlan)
#include "test_plan.moc"
