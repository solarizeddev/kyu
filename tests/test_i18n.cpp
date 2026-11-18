#include "aftercare.h"
#include "i18n.h"
#include "traystate.h"
#include "updatestate.h"

#include <QFileInfo>
#include <QTest>
#include <QTranslator>

using namespace Kyu;

// Kyu's words: English without a translation, German with the built
// kyu_de.qm (translations/kyu_de.ts, through lrelease).
class TestI18n : public QObject
{
    Q_OBJECT

private:
    static UpdateState updates(int packages)
    {
        UpdateState s;
        for (int i = 0; i < packages; ++i) {
            s.packages.append({QStringLiteral("p%1").arg(i), QStringLiteral("core"), QStringLiteral("1-1"), QStringLiteral("1-2")});
        }
        return s;
    }

private Q_SLOTS:
    void english()
    {
        QCOMPARE(numbered("%n updates available", "1 update available", 1), QStringLiteral("1 update available"));
        QCOMPARE(numbered("%n updates available", "1 update available", 3), QStringLiteral("3 updates available"));
        QCOMPARE(numbered("%n updates available", "1 update available", 0), QStringLiteral("0 updates available"));
        QCOMPARE(I18n::tr("Restart required: %1").arg(restartReasonText(QStringLiteral("new kernel 6.18.22-1-MANJARO"))),
                 QStringLiteral("Restart required: new kernel 6.18.22-1-MANJARO"));
        QCOMPARE(updatesHeading(updates(1)), QStringLiteral("1 update available"));
        QCOMPARE(updatesBreakdown(updates(2)), QStringLiteral("2 packages"));
    }

    // Under the C locale Kyu stays English, whatever LANGUAGE says (as
    // gettext, and pacman, do).
    void cLocaleStaysEnglish()
    {
        const QByteArray lcAll = qgetenv("LC_ALL"), language = qgetenv("LANGUAGE");
        qputenv("LC_ALL", "C.UTF-8");
        qputenv("LANGUAGE", "de");
        QVERIFY(!loadTranslations());
        QCOMPARE(I18n::tr("Up to date"), QStringLiteral("Up to date"));
        lcAll.isNull() ? qunsetenv("LC_ALL") : qputenv("LC_ALL", lcAll);
        language.isNull() ? qunsetenv("LANGUAGE") : qputenv("LANGUAGE", language);
    }

    void german()
    {
        const QString qm = QStringLiteral(KYU_TEST_QM_DIR "/kyu_de.qm");
        if (!QFileInfo::exists(qm)) {
            QSKIP("kyu_de.qm not built (Qt's Linguist tools missing)");
        }
        QTranslator translator;
        QVERIFY(translator.load(qm));
        QCoreApplication::installTranslator(&translator);
        QCOMPARE(numbered("%n updates available", "1 update available", 1), QStringLiteral("1 Aktualisierung verfügbar"));
        QCOMPARE(numbered("%n updates available", "1 update available", 3), QStringLiteral("3 Aktualisierungen verfügbar"));
        QCOMPARE(numbered("%n configuration files to merge", "1 configuration file to merge", 2), QStringLiteral("2 Konfigurationsdateien zusammenzuführen"));
        QCOMPARE(I18n::tr("Restart required: %1").arg(restartReasonText(QStringLiteral("new kernel 6.18.22-1-MANJARO"))),
                 QStringLiteral("Neustart erforderlich: neuer Kernel 6.18.22-1-MANJARO"));
        QCOMPARE(restartReasonText(QStringLiteral("systemd was updated")), QStringLiteral("systemd wurde aktualisiert"));
        // Not a reason Kyu records: as it is.
        QCOMPARE(restartReasonText(QStringLiteral("something else")), QStringLiteral("something else"));
        QCOMPARE(updatesHeading(updates(1)), QStringLiteral("1 Aktualisierung verfügbar"));
        QCOMPARE(updatesBreakdown(updates(2)), QStringLiteral("2 Pakete"));
        // Kyu's own keys stay in the prompts: it reads those letters.
        QVERIFY(I18n::tr("[c] clean up").startsWith(QLatin1String("[c] ")));
        QVERIFY(I18n::tr("Continue? [y/N] ").endsWith(QLatin1String("[y/N] ")));
        QCoreApplication::removeTranslator(&translator);
        QCOMPARE(numbered("%n updates available", "1 update available", 1), QStringLiteral("1 update available"));
    }
};

QTEST_GUILESS_MAIN(TestI18n)
#include "test_i18n.moc"
