#include "i18n.h"

#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

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

namespace
{
// gettext's rule, as pacman follows it: the first of LC_ALL, LC_MESSAGES and
// LANG that is set decides; under the C locale (or with none set) all is
// English, and LANGUAGE is not used.
bool cLocale()
{
    for (const char *name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const QByteArray value = qgetenv(name);
        if (!value.isEmpty()) {
            return value == "C" || value == "POSIX" || value.startsWith("C.");
        }
    }
    return true;
}
}

bool loadTranslations()
{
    if (!QCoreApplication::instance() || cLocale()) {
        return false;
    }
    QStringList dirs;
    if (qEnvironmentVariableIsSet("KYU_TRANSLATIONS_DIR")) {
        dirs.append(qEnvironmentVariable("KYU_TRANSLATIONS_DIR"));
    }
#ifdef KYU_BUILD_TRANSLATIONS
    dirs.append(QStringLiteral(KYU_BUILD_TRANSLATIONS));
#endif
    dirs.append(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/kyu/translations"));
#ifdef KYU_INSTALLED_TRANSLATIONS
    dirs.append(QStringLiteral(KYU_INSTALLED_TRANSLATIONS));
#endif
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
    if (language.isEmpty()) {
        return false;
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
