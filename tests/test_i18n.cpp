#include "aftercare.h"
#include "i18n.h"
#include "traystate.h"
#include "updatestate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTest>
#include <QTranslator>
#include <QXmlStreamReader>

using namespace Kyu;

// Kyu's words: English without a translation, German with the built
// kyu_de.qm (translations/kyu_de.ts, through lrelease).
class TestI18n : public QObject
{
    Q_OBJECT

private:
    // %1..%9 and %n in a text.
    static QSet<QString> placeholders(const QString &text)
    {
        static const QRegularExpression re(QStringLiteral("%[1-9n]"));
        QSet<QString> found;
        for (const QRegularExpressionMatch &m : re.globalMatch(text)) {
            found.insert(m.captured());
        }
        return found;
    }
    // Kyu's keys in brackets ("[c]", "[Enter]"), in order: Kyu reads those letters.
    static QStringList keys(const QString &text)
    {
        static const QRegularExpression re(QStringLiteral("\\[[^\\[\\]\\s/]+\\]"));
        QStringList found;
        for (const QRegularExpressionMatch &m : re.globalMatch(text)) {
            found.append(m.captured());
        }
        return found;
    }

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

    // A language whose locale is not generated: setlocale() left LC_MESSAGES
    // at C, so pacman stays English, and Kyu with it.
    void ungeneratedLocaleStaysEnglish()
    {
        QVERIFY(!staysEnglish("", "", "de_DE.UTF-8", "de_DE.UTF-8"));
        QVERIFY(!staysEnglish("", "de_DE.UTF-8", "C", "de_DE.UTF-8"));
        QVERIFY(staysEnglish("", "", "de_DE.UTF-8", "C"));
        QVERIFY(staysEnglish("de_DE.UTF-8", "", "", "POSIX"));
        QVERIFY(staysEnglish("de_DE.UTF-8", "", "", nullptr));
        // The C locale, or none set: English whatever setlocale() says.
        QVERIFY(staysEnglish("C.UTF-8", "", "de_DE.UTF-8", "de_DE.UTF-8"));
        QVERIFY(staysEnglish("", "POSIX", "de_DE.UTF-8", "de_DE.UTF-8"));
        QVERIFY(staysEnglish("", "", "", "de_DE.UTF-8"));
    }

    // The build tree's .qm files only for a program run from the build tree.
    void buildTranslationsOnlyInTheBuildTree()
    {
        const QString build = QStringLiteral("/nonexistent/kyu/build"), built = build + QStringLiteral("/translations");
        const QStringList installed{QStringLiteral("/nonexistent/usr/share/kyu/translations")};
        QCOMPARE(translationDirs(QString(), build + QStringLiteral("/bin"), build, built, installed), QStringList{built} + installed);
        QCOMPARE(translationDirs(QString(), QStringLiteral("/nonexistent/usr/bin"), build, built, installed), installed);
        // Only inside it: not a directory whose name starts the same.
        QCOMPARE(translationDirs(QString(), build + QStringLiteral("2/bin"), build, built, installed), installed);
        QCOMPARE(translationDirs(QString(), build + QStringLiteral("/bin"), QString(), QString(), installed), installed);
        // KYU_TRANSLATIONS_DIR first, always.
        QCOMPARE(translationDirs(QStringLiteral("/nonexistent/qm"), QStringLiteral("/nonexistent/usr/bin"), build, built, installed),
                 QStringList{QStringLiteral("/nonexistent/qm")} + installed);
    }

    // Every translation (translations/kyu_*.ts) keeps what Kyu fills in or
    // reads: the same placeholders (a singular may say "one" instead of %n),
    // the keys in brackets, and "[y/N]" with its trailing space.
    void translationsKeepPlaceholdersAndKeys()
    {
        const QFileInfoList files = QDir(QStringLiteral(KYU_TEST_TS_DIR)).entryInfoList({QStringLiteral("kyu_*.ts")}, QDir::Files);
        QVERIFY(!files.isEmpty());
        int checked = 0;
        for (const QFileInfo &file : files) {
            QFile ts(file.filePath());
            QVERIFY(ts.open(QIODevice::ReadOnly));
            QXmlStreamReader xml(&ts);
            QString source;
            QStringList forms;
            bool numerus = false;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QLatin1String("message")) {
                    source.clear();
                    forms.clear();
                    numerus = xml.attributes().value(QLatin1String("numerus")) == QLatin1String("yes");
                } else if (xml.isStartElement() && xml.name() == QLatin1String("source")) {
                    source = xml.readElementText();
                } else if (xml.isStartElement() && xml.name() == QLatin1String("translation") && !numerus) {
                    forms.append(xml.readElementText());
                } else if (xml.isStartElement() && xml.name() == QLatin1String("numerusform")) {
                    forms.append(xml.readElementText());
                } else if (xml.isEndElement() && xml.name() == QLatin1String("message")) {
                    for (qsizetype i = 0; i < forms.size(); ++i) {
                        const QString &form = forms.at(i);
                        if (form.isEmpty()) {
                            continue; // not translated yet: English
                        }
                        const QString where = file.fileName() + QStringLiteral(": \"") + source + QStringLiteral("\" → \"") + form + QLatin1Char('"');
                        QSet<QString> expected = placeholders(source);
                        if (numerus && i == 0 && !placeholders(form).contains(QStringLiteral("%n"))) {
                            expected.remove(QStringLiteral("%n"));
                        }
                        QVERIFY2(placeholders(form) == expected, qPrintable(QStringLiteral("placeholders differ in ") + where));
                        QVERIFY2(keys(form) == keys(source), qPrintable(QStringLiteral("keys differ in ") + where));
                        QVERIFY2(form.contains(QLatin1String("[y/N]")) == source.contains(QLatin1String("[y/N]")),
                                 qPrintable(QStringLiteral("[y/N] differs in ") + where));
                        QVERIFY2(form.endsWith(QLatin1String("[y/N] ")) == source.endsWith(QLatin1String("[y/N] ")),
                                 qPrintable(QStringLiteral("trailing \"[y/N] \" differs in ") + where));
                        ++checked;
                    }
                }
            }
            QVERIFY2(!xml.hasError(), qPrintable(file.fileName() + QStringLiteral(": ") + xml.errorString()));
        }
        QVERIFY(checked > 100);
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
