#include "waiting.h"

#include "i18n.h"
#include "paths.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace Kyu
{
QString waitingPath()
{
    return Paths::stateDir() + QStringLiteral("/waiting");
}

void setWaiting(const QString &text)
{
    if (text.isEmpty()) {
        QFile::remove(waitingPath());
        return;
    }
    QDir().mkpath(Paths::stateDir());
    QSaveFile file(waitingPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(text.toUtf8());
        file.commit();
    }
}

QString waiting()
{
    QFile file(waitingPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

QString waitingPrompt(const QByteArray &tail)
{
    QString text = QString::fromUtf8(tail);
    // Colours, cursor moves, titles, and charset switches ("ESC ( B", from
    // tput sgr0, which makepkg uses).
    static const QRegularExpression escapes(QStringLiteral("\\x1b(\\[[0-9;?]*[ -/]*[@-~]|\\][^\\x07\\x1b]*(\\x07|\\x1b\\\\)|[()][0-9A-Za-z]|[@-Z\\\\-_])"));
    text.remove(escapes);
    // The lines as they show: of a line redrawn with carriage returns (a
    // progress bar), its last drawing.
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        while (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        line = line.section(QLatin1Char('\r'), -1);
    }
    // pacman's "::" and makepkg's or paru's "==>" in front go.
    const auto bare = [](QString line) {
        line = line.trimmed();
        for (const QLatin1String mark : {QLatin1String("==>"), QLatin1String("::")}) {
            if (line.startsWith(mark)) {
                line = line.mid(mark.size()).trimmed();
            }
        }
        return line;
    };
    // The nearest earlier line that matches, among the last few.
    const auto earlier = [&lines, &bare](const auto &matches) {
        for (qsizetype n = lines.size() - 2; n >= 0 && n >= lines.size() - 8; --n) {
            if (matches(lines.at(n))) {
                return bare(lines.at(n));
            }
        }
        return QString();
    };
    const QString cursor = lines.constLast().trimmed();
    if (cursor.isEmpty()) {
        return QString(); // the output ended its line: nothing asked
    }
    const QString line = bare(cursor);
    const QString generic = I18n::tr("An answer is needed in the Kyu window.");
    // paru's and yay's menus: the question above, an empty "==> " to type at.
    if (line.isEmpty()) {
        const QString question = earlier([&bare](const QString &l) {
            return bare(l).endsWith(QLatin1Char('?'));
        });
        return question.isEmpty() ? generic : question;
    }
    // A pager (a PKGBUILD to review, a diff): less's ":" and "(END)", more's.
    static const QRegularExpression pager(QStringLiteral("^(:|\\(END\\)|lines \\d+-\\d+.*|.*--More--.*)$"));
    if (pager.match(line).hasMatch()) {
        return I18n::tr("Something to read in the Kyu window.");
    }
    // A fingerprint reader or a security key (PAM), in their own words.
    static const QRegularExpression confirm(QStringLiteral("finger|touch|token"), QRegularExpression::CaseInsensitiveOption);
    if (confirm.match(line).hasMatch()) {
        return I18n::tr("Your confirmation is needed.");
    }
    // A choice ("[Y/n]", "[J/n]", "[o/N]"), or a sentence ending in "?" or
    // ":" (a password, "Enter a selection (default=all):"). A progress bar
    // ends in "%" or "]" without a "/" in the brackets.
    static const QRegularExpression choice(QStringLiteral("\\[[^\\[\\]/\\s]{1,4}(/[^\\[\\]/\\s]{1,4})+\\]:?$"));
    if (!choice.match(line).hasMatch() && !line.endsWith(QLatin1Char('?')) && !line.endsWith(QLatin1Char(':'))) {
        return QString();
    }
    if (line.contains(QLatin1String("[sudo]")) || line.contains(QLatin1String("password"), Qt::CaseInsensitive) || line.startsWith(QLatin1String("doas ("))) {
        return I18n::tr("Your password is needed.");
    }
    // pacman's provider choice: the question is the "::" line above
    // ("There are 2 providers available for …").
    if (line.startsWith(QLatin1String("Enter a"))) {
        const QString question = earlier([](const QString &l) {
            return l.trimmed().startsWith(QLatin1String("::")) && l.trimmed().endsWith(QLatin1Char(':'));
        });
        if (!question.isEmpty()) {
            return question;
        }
    }
    return line.size() < 3 ? generic : line;
}
}
