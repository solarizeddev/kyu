#include "i18n.h"

#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include <clocale>

namespace Kyu
{
QString numbered(const char *many, const char *one, qsizetype n)
{
    const QString translated = QCoreApplication::translate("Kyu", many, one, int(n));
    const QString untranslated = QString::fromUtf8(many).replace(QLatin1String("%n"), QString::number(n));
    if (translated == untranslated && n == 1) {
        return QString::fromUtf8(one);
    }
    return translated;
}

bool staysEnglish(const QByteArray &lcAll, const QByteArray &lcMessages, const QByteArray &lang, const char *messagesLocale)
{
    // gettext's rule, as pacman follows it: the first of LC_ALL, LC_MESSAGES
    // and LANG that is set decides; under the C locale (or with none set) all
    // is English, and LANGUAGE is not used.
    bool c = true;
    for (const QByteArray *value : {&lcAll, &lcMessages, &lang}) {
        if (!value->isEmpty()) {
            c = *value == "C" || *value == "POSIX" || value->startsWith("C.");
            break;
        }
    }
    if (c) {
        return true;
    }
    // A locale that is not generated (de_DE.UTF-8 without locale-gen):
    // setlocale() leaves the C locale, and gettext programs stay English.
    const QByteArray messages(messagesLocale ? messagesLocale : "C");
    return messages == "C" || messages == "POSIX";
}

QStringList translationDirs(const QString &envDir, const QString &appDir, const QString &buildDir, const QString &buildTranslations,
                            const QStringList &installed)
{
    QStringList dirs;
    if (!envDir.isEmpty()) {
        dirs.append(envDir);
    }
    // The build tree's .qm files only for its own programs: an installed Kyu
    // never reads a build directory that happens to be on this computer.
    if (!buildDir.isEmpty() && !buildTranslations.isEmpty()) {
        const auto clean = [](const QString &path) {
            const QString canonical = QFileInfo(path).canonicalFilePath();
            return QDir::cleanPath(canonical.isEmpty() ? path : canonical);
        };
        const QString app = clean(appDir), build = clean(buildDir);
        if (app == build || app.startsWith(build + QLatin1Char('/'))) {
            dirs.append(buildTranslations);
        }
    }
    dirs.append(installed);
    return dirs;
}

bool loadTranslations(bool withQt)
{
    if (!QCoreApplication::instance()
        || staysEnglish(qgetenv("LC_ALL"), qgetenv("LC_MESSAGES"), qgetenv("LANG"), std::setlocale(LC_MESSAGES, nullptr))) {
        return false;
    }
    QStringList installed{QCoreApplication::applicationDirPath() + QStringLiteral("/../share/kyu/translations")};
#ifdef KYU_INSTALLED_TRANSLATIONS
    installed.append(QStringLiteral(KYU_INSTALLED_TRANSLATIONS));
#endif
    QString buildDir, buildTranslations;
#if defined(KYU_BUILD_DIR) && defined(KYU_BUILD_TRANSLATIONS)
    buildDir = QStringLiteral(KYU_BUILD_DIR);
    buildTranslations = QStringLiteral(KYU_BUILD_TRANSLATIONS);
#endif
    const QStringList dirs = translationDirs(qEnvironmentVariable("KYU_TRANSLATIONS_DIR"), QCoreApplication::applicationDirPath(), buildDir,
                                             buildTranslations, installed);
    // The user's languages in order (LANGUAGE first, as Qt reads it): the
    // first one Kyu has, unless English comes before it ("en_US:de").
    QString language;
    for (QString name : QLocale().uiLanguages()) {
        name.replace(QLatin1Char('-'), QLatin1Char('_'));
        if (name.startsWith(QLatin1String("en")) || name == QLatin1String("C")) {
            break;
        }
        auto *kyu = new QTranslator(QCoreApplication::instance());
        for (const QString &dir : std::as_const(dirs)) {
            // kyu_de_DE.qm, else kyu_de.qm.
            if (kyu->load(QStringLiteral("kyu_") + name, dir)) {
                language = name;
                break;
            }
        }
        if (!language.isEmpty()) {
            QCoreApplication::installTranslator(kyu);
            break;
        }
        delete kyu;
    }
    if (language.isEmpty() || !withQt) {
        return !language.isEmpty();
    }
    // Qt's own words (dialog buttons, --help) in the same language.
    auto *qt = new QTranslator(QCoreApplication::instance());
    if (qt->load(QStringLiteral("qtbase_") + language, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(qt);
    } else {
        delete qt;
    }
    return true;
}
}
