// pacman's failures recognised in another language, with pacman's own
// catalogs (here German ones shaped like them: tests/update/make-mo.py).
#include "failure.h"
#include "translations.h"

#include <QJsonArray>
#include <QTest>

using namespace Kyu;

class TestTranslations : public QObject
{
    Q_OBJECT
private:
    static PacmanTranslations german()
    {
        const QString dir = QStringLiteral(KYU_TEST_LOCALE "/de/LC_MESSAGES/");
        return PacmanTranslations::fromCatalogs({dir + QStringLiteral("libalpm.mo"), dir + QStringLiteral("pacman.mo")});
    }
    static QString kind(const QStringList &lines)
    {
        return classifyFailure(german().toEnglish(lines)).value(QStringLiteral("kind")).toString();
    }

private Q_SLOTS:
    void catalog()
    {
        const auto c = readMoFile(QStringLiteral(KYU_TEST_LOCALE "/de/LC_MESSAGES/libalpm.mo"));
        QCOMPARE(c.value(QStringLiteral("conflicting files")), QStringLiteral("In Konflikt stehende Dateien"));
        QVERIFY(readMoFile(QStringLiteral("/nonexistent.mo")).isEmpty());
    }

    void lines()
    {
        const PacmanTranslations t = german();
        // Nested: the message pacman filled in is translated too.
        QCOMPARE(t.toEnglish({QStringLiteral("Fehler: Konnte den Vorgang nicht durchführen (Ungültiges oder beschädigtes Paket (PGP-Signatur))")}),
                 QStringList{QStringLiteral("error: failed to commit transaction (invalid or corrupted package (PGP signature))")});
        // Arguments in another order come back in pacman's.
        QCOMPARE(t.toEnglish({QStringLiteral("Fehler: Partition /var zu voll: 12 Blöcke frei, 300 Blöcke benötigt")}),
                 QStringList{QStringLiteral("error: Partition /var too full: 300 blocks needed, 12 blocks free")});
        // Unknown lines stay as they were, indentation and all.
        QCOMPARE(t.toEnglish({QStringLiteral("  Pakete (2) mesa-1"), QString()}), (QStringList{QStringLiteral("  Pakete (2) mesa-1"), QString()}));
    }

    void kinds()
    {
        QCOMPARE(kind({QStringLiteral("Fehler: mesa: Signatur von \"Someone <s@example.org>\" hat unbekanntes Vertrauen"),
                       QStringLiteral("Fehler: Konnte den Vorgang nicht durchführen (Ungültiges oder beschädigtes Paket (PGP-Signatur))"),
                       QStringLiteral("Fehler aufgetreten, keine Pakete wurden aktualisiert.")}),
                 QStringLiteral("keyring"));
        QCOMPARE(kind({QStringLiteral("Fehler: Konnte den Vorgang nicht durchführen (In Konflikt stehende Dateien)"),
                       QStringLiteral("foo: /usr/bin/foo existiert im Dateisystem")}),
                 QStringLiteral("files"));
        QCOMPARE(kind({QStringLiteral("Fehler: Konnte den Vorgang nicht starten (Konnte die Datenbank nicht sperren)"),
                       QStringLiteral("Fehler: Konnte die Datenbank nicht sperren: Datei existiert bereits")}),
                 QStringLiteral("lock"));
        QCOMPARE(kind({QStringLiteral("Fehler: Partition / zu voll: 1 Blöcke frei, 9 Blöcke benötigt"),
                       QStringLiteral("Fehler: Konnte den Vorgang nicht durchführen (Nicht genügend freier Speicherplatz)")}),
                 QStringLiteral("disk"));
        // curl's reasons are English whatever the language.
        QCOMPARE(kind({QStringLiteral("Fehler: Konnte Datei 'core.db' nicht von mirror.example.org übertragen : Could not resolve host: mirror.example.org")}),
                 QStringLiteral("network"));
        const QJsonObject mirror = classifyFailure(german().toEnglish(
            {QStringLiteral("Fehler: Konnte Datei 'mesa-1-x86_64.pkg.tar.zst' nicht von m.example.org übertragen : The requested URL returned error: 404")}));
        QCOMPARE(mirror.value(QStringLiteral("kind")).toString(), QStringLiteral("mirror"));
        QCOMPARE(mirror.value(QStringLiteral("hosts")).toArray().first().toString(), QStringLiteral("m.example.org"));
    }

    void environment()
    {
        const QString dir = QStringLiteral(KYU_TEST_LOCALE);
        const QByteArray lcAll = qgetenv("LC_ALL"), lcMessages = qgetenv("LC_MESSAGES"), lang = qgetenv("LANG"), language = qgetenv("LANGUAGE");
        qunsetenv("LC_ALL");
        qunsetenv("LC_MESSAGES");
        qunsetenv("LANGUAGE");
        qputenv("LANG", "de_DE.UTF-8");
        QCOMPARE(PacmanTranslations::forEnvironment(dir).languages(), QStringList{QStringLiteral("de")});
        qputenv("LANGUAGE", "en_GB:de");
        QVERIFY(PacmanTranslations::forEnvironment(dir).isEmpty()); // English first
        qunsetenv("LANGUAGE");
        qputenv("LC_ALL", "C.UTF-8");
        QVERIFY(PacmanTranslations::forEnvironment(dir).isEmpty());
        qputenv("LC_ALL", "fr_FR.UTF-8");
        QVERIFY(PacmanTranslations::forEnvironment(dir).isEmpty()); // no French catalog here
        const auto restore = [](const char *name, const QByteArray &value) {
            if (value.isNull()) {
                qunsetenv(name);
            } else {
                qputenv(name, value);
            }
        };
        restore("LC_ALL", lcAll);
        restore("LC_MESSAGES", lcMessages);
        restore("LANG", lang);
        restore("LANGUAGE", language);
    }
};

QTEST_GUILESS_MAIN(TestTranslations)
#include "test_translations.moc"
