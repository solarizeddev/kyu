#include "translations.h"

#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <algorithm>
#include <optional>

namespace Kyu
{
namespace
{
// The English words Kyu recognises failures by (failure.cpp), and what
// frames them: only messages with one of these are worth turning back.
const char *const Needles[] = {
    "failed retrieving file", "failed to synchronize", "unable to lock database", "could not lock database", "failed to init transaction",
    "failed to commit transaction", "failed to prepare transaction", "unknown trust", "marginal trust", "is invalid", "PGP signature",
    "required key missing from keyring", "could not be looked up remotely", "signature from", "conflicting files",
    "exists in filesystem", "too full", "not enough free disk space", "could not satisfy dependencies", "unable to satisfy dependency",
    "breaks dependency", "Errors occurred", "invalid or corrupted package", "download library error", "exists in both",
};

// printf conversions: %s, %d, %jd, %zu, %1$s... (and %% for a percent sign).
const QRegularExpression &conversion()
{
    static const QRegularExpression re(QStringLiteral("%(?:(\\d+)\\$)?[-+ #0']*\\d*(?:\\.\\d+)?(?:hh|h|ll|l|j|z|t|L)?([sdiucfgxXoeEp%])"));
    return re;
}

// The English original with %1, %2... for its conversions, in order.
QString numbered(const QString &english)
{
    QString out;
    qsizetype at = 0;
    int n = 0;
    auto it = conversion().globalMatch(english);
    while (it.hasNext()) {
        const auto m = it.next();
        out += english.mid(at, m.capturedStart() - at);
        if (m.captured(2) == QLatin1String("%")) {
            out += QLatin1Char('%');
        } else {
            const int index = m.captured(1).isEmpty() ? ++n : m.captured(1).toInt();
            out += QStringLiteral("\x01%1\x02").arg(index);
        }
        at = m.capturedEnd();
    }
    out += english.mid(at);
    return out;
}

// The translation as a regular expression: a group per conversion, in the
// translation's order; which original argument each group is.
std::optional<std::pair<QRegularExpression, QList<int>>> pattern(const QString &translated)
{
    QString re = QStringLiteral("^");
    QList<int> order;
    qsizetype at = 0, literal = 0;
    int n = 0;
    auto it = conversion().globalMatch(translated);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString text = translated.mid(at, m.capturedStart() - at);
        literal += text.trimmed().size();
        re += QRegularExpression::escape(text);
        if (m.captured(2) == QLatin1String("%")) {
            re += QStringLiteral("%");
        } else {
            re += QStringLiteral("(.*?)");
            order.append(m.captured(1).isEmpty() ? ++n : m.captured(1).toInt());
        }
        at = m.capturedEnd();
    }
    const QString rest = translated.mid(at);
    literal += rest.trimmed().size();
    re += QRegularExpression::escape(rest) + QStringLiteral("$");
    // Too little of its own to be recognised safely ("%s: %s").
    if (literal < 4) {
        return std::nullopt;
    }
    QRegularExpression compiled(re);
    if (!compiled.isValid()) {
        return std::nullopt;
    }
    return std::make_pair(compiled, order);
}

// gettext's choice of catalog names for a locale: de_DE.UTF-8@euro →
// de_DE@euro, de_DE, de@euro, de.
QStringList candidates(const QString &locale)
{
    QString base = locale, modifier;
    if (const qsizetype at = base.indexOf(QLatin1Char('@')); at >= 0) {
        modifier = base.mid(at);
        base.truncate(at);
    }
    if (const qsizetype dot = base.indexOf(QLatin1Char('.')); dot >= 0) {
        base.truncate(dot);
    }
    const QString language = base.section(QLatin1Char('_'), 0, 0);
    QStringList out;
    for (const QString &c : {base + modifier, base, language + modifier, language}) {
        if (!c.isEmpty() && !out.contains(c)) {
            out.append(c);
        }
    }
    return out;
}
}

QHash<QString, QString> readMoFile(const QString &path)
{
    QHash<QString, QString> out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) {
        return out;
    }
    const QByteArray data = file.readAll();
    if (data.size() < 28) {
        return out;
    }
    const auto u32 = [&data](qsizetype at, bool little) -> quint32 {
        if (at < 0 || at + 4 > data.size()) {
            return 0;
        }
        return little ? qFromLittleEndian<quint32>(data.constData() + at) : qFromBigEndian<quint32>(data.constData() + at);
    };
    bool little = true;
    if (u32(0, true) != 0x950412de) {
        if (u32(0, false) != 0x950412de) {
            return out;
        }
        little = false;
    }
    const quint32 count = u32(8, little), originals = u32(12, little), translations = u32(16, little);
    const auto string = [&](quint32 table, quint32 i) -> std::optional<QByteArray> {
        const quint32 length = u32(qsizetype(table) + i * 8, little), offset = u32(qsizetype(table) + i * 8 + 4, little);
        if (qsizetype(offset) + length > data.size()) {
            return std::nullopt;
        }
        return data.mid(offset, length);
    };
    for (quint32 i = 0; i < count && i < 100000; ++i) {
        const auto id = string(originals, i), str = string(translations, i);
        if (!id || !str || id->isEmpty()) {
            continue; // the header has an empty original
        }
        // A context ("ctx\x04id") is left off; plural forms: the singular
        // original with its first form, the plural with the others.
        QByteArray original = *id;
        if (const qsizetype ctx = original.indexOf('\x04'); ctx >= 0) {
            original = original.mid(ctx + 1);
        }
        const QList<QByteArray> ids = original.split('\0');
        const QList<QByteArray> forms = str->split('\0');
        for (qsizetype f = 0; f < forms.size(); ++f) {
            const QString english = QString::fromUtf8(ids.value(f == 0 ? 0 : ids.size() - 1));
            const QString translated = QString::fromUtf8(forms.at(f));
            if (!translated.isEmpty() && !out.contains(english)) {
                out.insert(english, translated);
            }
        }
    }
    return out;
}

void PacmanTranslations::add(const QHash<QString, QString> &catalog)
{
    for (auto it = catalog.cbegin(); it != catalog.cend(); ++it) {
        const QString english = it.key();
        const QString translated = it.value();
        // pacman's "error: " and "warning: " come before the message.
        if (english == QLatin1String("error: ") || english == QLatin1String("warning: ")) {
            if (translated != english) {
                m_prefixes.append({translated, english});
            }
            continue;
        }
        if (translated == english) {
            continue;
        }
        // One line each: what pacman prints ends in newlines at most.
        QString e = english, t = translated;
        while (e.endsWith(QLatin1Char('\n'))) {
            e.chop(1);
        }
        while (t.endsWith(QLatin1Char('\n'))) {
            t.chop(1);
        }
        if (e.contains(QLatin1Char('\n')) || t.contains(QLatin1Char('\n'))) {
            continue;
        }
        if (std::none_of(std::begin(Needles), std::end(Needles), [&e](const char *n) {
                return e.contains(QLatin1String(n));
            })) {
            continue;
        }
        const auto p = pattern(t.trimmed());
        if (!p) {
            continue;
        }
        // The groups name original arguments; the English text gets them by number.
        QString target = numbered(e.trimmed());
        Pattern entry{p->first, {}};
        // Rewrite \x01N\x02 to the group that holds argument N.
        for (int g = 0; g < p->second.size(); ++g) {
            target.replace(QStringLiteral("\x01%1\x02").arg(p->second.at(g)), QStringLiteral("\x03%1\x04").arg(g + 1));
        }
        entry.english = target;
        m_patterns.append(entry);
    }
    // Longer translations first: the most specific one wins.
    std::stable_sort(m_patterns.begin(), m_patterns.end(), [](const Pattern &a, const Pattern &b) {
        return a.translated.pattern().size() > b.translated.pattern().size();
    });
    std::stable_sort(m_prefixes.begin(), m_prefixes.end(), [](const auto &a, const auto &b) {
        return a.first.size() > b.first.size();
    });
}

QString PacmanTranslations::line(const QString &text, int depth) const
{
    for (const Pattern &p : m_patterns) {
        const auto m = p.translated.match(text);
        if (!m.hasMatch()) {
            continue;
        }
        QString out = p.english;
        for (int g = 1; g <= m.lastCapturedIndex(); ++g) {
            // What pacman filled in may be a translated message itself
            // ("failed to commit transaction (%s)"), with whatever spaces
            // its translator left around it (French: a no-break space).
            const QString captured = m.captured(g).trimmed();
            const QString arg = depth < 2 ? line(captured, depth + 1) : captured;
            out.replace(QStringLiteral("\x03%1\x04").arg(g), arg);
        }
        // An argument the translation does not use.
        static const QRegularExpression unused(QStringLiteral("[\\x01\\x03]\\d+[\\x02\\x04]"));
        out.remove(unused);
        return out;
    }
    return text;
}

QStringList PacmanTranslations::toEnglish(const QStringList &lines) const
{
    if (isEmpty()) {
        return lines;
    }
    QStringList out;
    out.reserve(lines.size());
    for (const QString &raw : lines) {
        // Very long lines are not pacman's messages (and cost time to match).
        if (raw.size() > 2000) {
            out.append(raw);
            continue;
        }
        // Keep the indentation; translate the rest.
        qsizetype indent = 0;
        while (indent < raw.size() && raw.at(indent).isSpace()) {
            ++indent;
        }
        QString text = raw.mid(indent).trimmed();
        QString prefix;
        bool changed = false;
        // pacman's ":: " lines (a dependency that cannot be satisfied).
        if (text.startsWith(QLatin1String(":: "))) {
            prefix = QStringLiteral(":: ");
            text = text.mid(3).trimmed();
        }
        for (const auto &[translated, english] : m_prefixes) {
            if (text.startsWith(translated.trimmed())) {
                prefix += english;
                text = text.mid(translated.trimmed().size()).trimmed();
                changed = translated != english;
                break;
            }
        }
        const QString message = line(text, 0);
        changed = changed || message != text;
        out.append(changed ? raw.left(indent) + prefix + message : raw);
    }
    return out;
}

PacmanTranslations PacmanTranslations::fromCatalogs(const QStringList &moFiles)
{
    PacmanTranslations t;
    for (const QString &file : moFiles) {
        t.add(readMoFile(file));
    }
    return t;
}

PacmanTranslations PacmanTranslations::forEnvironment(const QString &localeDir)
{
    // gettext: the locale from LC_ALL, LC_MESSAGES, LANG; with a locale that
    // is not C, LANGUAGE's list comes first, each tried in turn until one
    // has a catalog (English has none but en_GB: it falls through too).
    QString locale;
    for (const char *name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        locale = qEnvironmentVariable(name);
        if (!locale.isEmpty()) {
            break;
        }
    }
    if (locale.isEmpty() || locale == QLatin1String("C") || locale == QLatin1String("POSIX") || locale.startsWith(QLatin1String("C."))) {
        return {};
    }
    QStringList languages = qEnvironmentVariable("LANGUAGE").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    languages.append(locale);
    PacmanTranslations t;
    for (const QString &domain : {QStringLiteral("libalpm"), QStringLiteral("pacman")}) {
        bool found = false;
        for (const QString &language : std::as_const(languages)) {
            if (language == QLatin1String("C") || language == QLatin1String("POSIX")) {
                found = true;
                break;
            }
            for (const QString &c : candidates(language)) {
                const QString path = QStringLiteral("%1/%2/LC_MESSAGES/%3.mo").arg(localeDir, c, domain);
                if (QFileInfo::exists(path)) {
                    t.add(readMoFile(path));
                    if (!t.m_languages.contains(c)) {
                        t.m_languages.append(c);
                    }
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    }
    return t;
}
}
