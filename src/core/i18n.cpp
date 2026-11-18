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

bool loadTranslations()
{
    if (!QCoreApplication::instance()) {
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
    const QLocale locale;
    bool loaded = false;
    for (const QString &dir : std::as_const(dirs)) {
        auto *kyu = new QTranslator(QCoreApplication::instance());
        if (kyu->load(locale, QStringLiteral("kyu"), QStringLiteral("_"), dir)) {
            QCoreApplication::installTranslator(kyu);
            loaded = true;
            break;
        }
        delete kyu;
    }
    auto *qt = new QTranslator(QCoreApplication::instance());
    if (qt->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        QCoreApplication::installTranslator(qt);
    } else {
        delete qt;
    }
    return loaded;
}
}
