#pragma once

#include <QCoreApplication>
#include <QByteArray>
#include <QString>
#include <QStringList>

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
// from KYU_TRANSLATIONS_DIR, the build tree for a program run from there,
// else the installed share/kyu/translations) and, with withQt, Qt's own
// (dialog buttons, --help). None for English, nor where gettext programs
// stay English (staysEnglish()). Returns whether Kyu's loaded.
bool loadTranslations(bool withQt = true);

// Whether Kyu stays English, as gettext programs (pacman) do: under the C
// locale (the first of LC_ALL, LC_MESSAGES and LANG that is set; none set
// counts as C), whatever LANGUAGE says; and where the locale asked for is
// not generated, so that setlocale(LC_ALL, "") left LC_MESSAGES at "C"
// (messagesLocale: setlocale(LC_MESSAGES, nullptr), null when unknown).
bool staysEnglish(const QByteArray &lcAll, const QByteArray &lcMessages, const QByteArray &lang, const char *messagesLocale);

// Where kyu_LANG.qm is looked for, in order: envDir (KYU_TRANSLATIONS_DIR)
// when set; the build tree's translations only for a program inside the
// build tree (appDir under buildDir); then the installed ones.
QStringList translationDirs(const QString &envDir, const QString &appDir, const QString &buildDir, const QString &buildTranslations,
                            const QStringList &installed);
}
