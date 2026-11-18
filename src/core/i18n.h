#pragma once

#include <QCoreApplication>
#include <QString>

namespace Kyu
{
// Kyu's own words go through Qt's translations (translations/kyu_*.ts),
// all in one context, "Kyu": I18n::tr("Up to date"). pacman's, sudo's and
// flatpak's lines stay as those programs print them.
class I18n
{
    Q_DECLARE_TR_FUNCTIONS(Kyu)
};

// A count in a sentence: numbered("%n updates available", "1 update available", n).
// Translated with the language's plural forms; without a translation, the
// English one: the second text for 1 (lupdate shows it to translators as a
// comment). lupdate reads it through -tr-function-alias tr+=numbered.
QString numbered(const char *many, const char *one, qsizetype n);

// Installs the translations for the user's language: Kyu's (kyu_LANG.qm,
// from KYU_TRANSLATIONS_DIR, the build tree or the installed share/kyu/translations)
// and Qt's own (dialog buttons). None for English. Returns whether Kyu's loaded.
bool loadTranslations();
}
