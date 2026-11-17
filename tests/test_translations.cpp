// pacman's failures recognised in other languages, with pacman's own
// translations (tests/update/locale, written by tests/update/make-mo.py from
// pacman's real translations of these messages).
#include "failure.h"
#include "translations.h"

#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

using namespace Kyu;

namespace
{
PacmanTranslations catalogs(const char *language)
{
    const QString dir = QStringLiteral(KYU_TEST_LOCALE "/%1/LC_MESSAGES/").arg(QLatin1String(language));
    return PacmanTranslations::fromCatalogs({dir + QStringLiteral("libalpm.mo"), dir + QStringLiteral("pacman.mo")});
}

QJsonObject classify(const char *language, const QStringList &lines)
{
    return classifyFailure(catalogs(language).toEnglish(lines));
}

// A .mo file: entries in order, little or big endian.
void writeMo(const QString &path, const QList<std::pair<QByteArray, QByteArray>> &entries, bool big)
{
    QByteArray data;
    const quint32 n = quint32(entries.size());
    const quint32 start = 28 + n * 16;
    QByteArray strings;
    QList<std::pair<quint32, quint32>> ids, strs;
    for (const auto &e : entries) {
        ids.append({quint32(e.first.size()), start + quint32(strings.size())});
        strings += e.first + '\0';
    }
    for (const auto &e : entries) {
        strs.append({quint32(e.second.size()), start + quint32(strings.size())});
        strings += e.second + '\0';
    }
    const auto put = [&data, big](quint32 v) {
        char b[4];
        if (big) {
            qToBigEndian(v, b);
        } else {
            qToLittleEndian(v, b);
        }
        data.append(b, 4);
    };
    for (const quint32 v : {0x950412deu, 0u, n, 28u, 28 + n * 8, 0u, 0u}) {
        put(v);
    }
    for (const auto &[l, o] : ids) {
        put(l);
        put(o);
    }
    for (const auto &[l, o] : strs) {
        put(l);
        put(o);
    }
    data += strings;
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
}

class TestTranslations : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void german()
    {
        // Nested: the message pacman filled in is translated too.
        QCOMPARE(catalogs("de").toEnglish({QStringLiteral("Fehler: Der Vorgang konnte nicht durchgeführt werden (Ungültiges oder beschädigtes Paket (PGP-Signatur))")}),
                 QStringList{QStringLiteral("error: failed to commit transaction (invalid or corrupted package (PGP signature))")});
        // German pacman leaves this one in English: recognised as it is.
        QCOMPARE(classify("de", {QStringLiteral("Fehler: mesa: signature from \"Someone <s@example.org>\" is unknown trust"),
                                 QStringLiteral("Fehler: Der Vorgang konnte nicht durchgeführt werden (Ungültiges oder beschädigtes Paket (PGP-Signatur))"),
                                 QStringLiteral("Fehler sind aufgetreten, keine Pakete wurden aktualisiert.")})
                     .value(QStringLiteral("kind")),
                 QStringLiteral("keyring"));
        const QJsonObject files = classify("de", {QStringLiteral("Fehler: Der Vorgang konnte nicht durchgeführt werden (In Konflikt stehende Dateien)"),
                                                  QStringLiteral("foo: /usr/bin/foo existiert im Dateisystem (gehört zu bar)"),
                                                  QStringLiteral("/usr/lib/x existiert in »a« und »b«")});
        QCOMPARE(files.value(QStringLiteral("kind")), QStringLiteral("files"));
        QCOMPARE(files.value(QStringLiteral("fileCount")).toInt(), 2);
        QCOMPARE(files.value(QStringLiteral("files")).toArray().first().toString(), QStringLiteral("foo: /usr/bin/foo"));
        QCOMPARE(classify("de", {QStringLiteral("Fehler: Der Vorgang konnte nicht gestartet werden (Kann Datenbank nicht sperren)"),
                                 QStringLiteral("Fehler: Datenbank konnte nicht gesperrt werden: Die Datei existiert bereits")})
                     .value(QStringLiteral("kind")),
                 QStringLiteral("lock"));
        QCOMPARE(classify("de", {QStringLiteral("Fehler: Partition / ist überfüllt: 9 Blöcke werden benötigt, 1 Blöcke sind frei")}).value(QStringLiteral("kind")),
                 QStringLiteral("disk"));
        // curl's reasons are English whatever the language.
        QCOMPARE(classify("de", {QStringLiteral("Fehler: Konnte Datei 'core.db' nicht von m.example.org übertragen : Could not resolve host: m.example.org")})
                     .value(QStringLiteral("kind")),
                 QStringLiteral("network"));
    }

    void spanish()
    {
        // Spanish pacman keeps "error: " as it is.
        QCOMPARE(classify("es", {QStringLiteral("error: mesa: se desconoce el nivel de confianza de la firma de «Someone <s@example.org>»"),
                                 QStringLiteral("error: no se pudo realizar la operación (paquete no válido o dañado (firma PGP))")})
                     .value(QStringLiteral("kind")),
                 QStringLiteral("keyring"));
        QCOMPARE(classify("es", {QStringLiteral("error: no se pudo iniciar la operación (no se pudo bloquear la base de datos)")}).value(QStringLiteral("kind")),
                 QStringLiteral("lock"));
        const QJsonObject mirror = classify(
            "es", {QStringLiteral("error: no se pudo obtener el archivo «mesa-1-x86_64.pkg.tar.zst» desde m.example.org: The requested URL returned error: 404")});
        QCOMPARE(mirror.value(QStringLiteral("kind")), QStringLiteral("mirror"));
    }

    void french()
    {
        // No-break spaces, one left at the end of a translation.
        const QJsonObject files = classify("fr", {QStringLiteral("erreur : la validation de la transaction a échoué (conflit de fichiers )"),
                                                  QStringLiteral("foo : /usr/bin/foo est déjà présent dans le système de fichiers")});
        QCOMPARE(files.value(QStringLiteral("kind")), QStringLiteral("files"));
        QCOMPARE(files.value(QStringLiteral("fileCount")).toInt(), 1);
        QCOMPARE(classify("fr", {QStringLiteral("erreur : échec de récupération du fichier « core.db » depuis m.example.org : Connection timed out after 10000 milliseconds")})
                     .value(QStringLiteral("kind")),
                 QStringLiteral("network"));
    }

    void turkishAndChinese()
    {
        // Arguments in another order (%3$s first) come back in pacman's.
        QCOMPARE(catalogs("tr").toEnglish({QStringLiteral("hata: Could not resolve host: m.example.org hatası nedeniyle 'core.db' dosyası m.example.org adresinden alınamadı")}),
                 QStringList{QStringLiteral("error: failed retrieving file 'core.db' from m.example.org : Could not resolve host: m.example.org")});
        QCOMPARE(classify("tr", {QStringLiteral("hata: işlem gerçekleştirilemedi (yetersiz disk alanı)")}).value(QStringLiteral("kind")), QStringLiteral("disk"));
        // A full-width colon, no space.
        QCOMPARE(classify("zh_CN", {QStringLiteral("错误：mesa: 来自 \"Someone <s@example.org>\" 的签名是未知信任的"),
                                    QStringLiteral("错误：无法提交处理 (无效或已损坏的软件包 (PGP 签名))")})
                     .value(QStringLiteral("kind")),
                 QStringLiteral("keyring"));
        QCOMPARE(classify("zh_CN", {QStringLiteral("错误：无法从 m.example.org : Connection refused 获取文件 'core.db'")}).value(QStringLiteral("kind")),
                 QStringLiteral("network"));
    }

    void untouched()
    {
        // Lines that are not known messages stay as they were; English
        // lines (pacman run without the locale, as doas does) still work.
        const PacmanTranslations t = catalogs("de");
        QCOMPARE(t.toEnglish({QStringLiteral("  Pakete (2) mesa-1"), QString(), QStringLiteral(":: Fortfahren? [J/n]")}),
                 (QStringList{QStringLiteral("  Pakete (2) mesa-1"), QString(), QStringLiteral(":: Fortfahren? [J/n]")}));
        QCOMPARE(classify("de", {QStringLiteral("error: failed to init transaction (unable to lock database)")}).value(QStringLiteral("kind")),
                 QStringLiteral("lock"));
    }

    void moFiles()
    {
        QTemporaryDir dir;
        const QList<std::pair<QByteArray, QByteArray>> entries{
            {QByteArray(), "Content-Type: text/plain; charset=UTF-8\n"},
            {"conflicting files", "Dateikonflikt"},
            {QByteArrayLiteral("ctx\x04" "not enough free disk space"), "kein Platz"},
            {QByteArrayLiteral("%d file\0%d files"), QByteArrayLiteral("%d Datei\0%d Dateien")},
        };
        for (const bool big : {false, true}) {
            const QString path = dir.filePath(big ? QStringLiteral("be.mo") : QStringLiteral("le.mo"));
            writeMo(path, entries, big);
            const auto c = readMoFile(path);
            QCOMPARE(c.value(QStringLiteral("conflicting files")), QStringLiteral("Dateikonflikt"));
            QCOMPARE(c.value(QStringLiteral("not enough free disk space")), QStringLiteral("kein Platz")); // context left off
            QCOMPARE(c.value(QStringLiteral("%d file")), QStringLiteral("%d Datei"));
            QCOMPARE(c.value(QStringLiteral("%d files")), QStringLiteral("%d Dateien"));
        }
        // Truncated, or not a catalog at all: nothing, no crash.
        QFile whole(dir.filePath(QStringLiteral("le.mo")));
        QVERIFY(whole.open(QIODevice::ReadOnly));
        const QByteArray data = whole.readAll();
        for (const qsizetype size : {qsizetype(10), qsizetype(40), data.size() - 5}) {
            QFile cut(dir.filePath(QStringLiteral("cut.mo")));
            QVERIFY(cut.open(QIODevice::WriteOnly | QIODevice::Truncate));
            cut.write(data.left(size));
            cut.close();
            readMoFile(cut.fileName());
        }
        QVERIFY(readMoFile(QStringLiteral(KYU_TEST_LOCALE "/../make-mo.py")).isEmpty());
        QVERIFY(readMoFile(QStringLiteral("/nonexistent.mo")).isEmpty());
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
        // As gettext: a language without a catalog (English) falls through.
        qputenv("LANGUAGE", "en_US:de");
        QCOMPARE(PacmanTranslations::forEnvironment(dir).languages(), QStringList{QStringLiteral("de")});
        qputenv("LANGUAGE", "es:de");
        QCOMPARE(PacmanTranslations::forEnvironment(dir).languages(), QStringList{QStringLiteral("es")});
        qunsetenv("LANGUAGE");
        qputenv("LANG", "zh_CN.UTF-8");
        QCOMPARE(PacmanTranslations::forEnvironment(dir).languages(), QStringList{QStringLiteral("zh_CN")});
        // The C locale: LANGUAGE is not used, nothing to turn back.
        qputenv("LANGUAGE", "de");
        qputenv("LC_ALL", "C.UTF-8");
        QVERIFY(PacmanTranslations::forEnvironment(dir).isEmpty());
        qunsetenv("LANGUAGE");
        qputenv("LC_ALL", "it_IT.UTF-8");
        QVERIFY(PacmanTranslations::forEnvironment(dir).isEmpty()); // no Italian catalog here
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
