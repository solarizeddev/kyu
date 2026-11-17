#pragma once

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QStringList>

#include <utility>

namespace Kyu
{
// pacman speaks the person's language; Kyu recognises its failures by their
// English words. This turns pacman's translated lines back into English with
// pacman's own translations (its gettext catalogs, pacman.mo and libalpm.mo),
// so the failure is recognised in any language pacman is translated to.
// Only for recognising: what pacman printed is shown as it was.
class PacmanTranslations
{
public:
    // The catalogs for the language pacman uses with this environment
    // (LANGUAGE, LC_ALL, LC_MESSAGES, LANG, as gettext picks them), from
    // localeDir. None for English or the C locale, or when there are none.
    static PacmanTranslations forEnvironment(const QString &localeDir = QStringLiteral("/usr/share/locale"));

    // From catalog files directly (for tests).
    static PacmanTranslations fromCatalogs(const QStringList &moFiles);

    bool isEmpty() const
    {
        return m_patterns.isEmpty();
    }
    QStringList languages() const
    {
        return m_languages;
    }

    // The lines in English where a translation is known (with what pacman
    // filled in kept, itself turned back where it is a known message too);
    // the others as they were.
    QStringList toEnglish(const QStringList &lines) const;

private:
    struct Pattern {
        QRegularExpression translated; // the translation, its % places as groups
        QString english; // the original, with %1, %2... in their places
    };
    QString line(const QString &text, int depth) const;
    void add(const QHash<QString, QString> &catalog);

    QList<Pattern> m_patterns;
    // "Fehler: " → "error: "; the English ones always (Spanish keeps "error: ").
    QList<std::pair<QString, QString>> m_prefixes{{QStringLiteral("error: "), QStringLiteral("error: ")},
                                                  {QStringLiteral("warning: "), QStringLiteral("warning: ")}};
    QStringList m_languages;
};

// A gettext .mo file: original → translation. Empty when unreadable.
QHash<QString, QString> readMoFile(const QString &path);
}
