#include "waiting.h"

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
    // The line the cursor is on: after the last newline, and of a line
    // redrawn with carriage returns (a progress bar), its last drawing.
    QString text = QString::fromUtf8(tail);
    static const QRegularExpression escapes(QStringLiteral("\\x1b(\\[[0-9;?]*[ -/]*[@-~]|\\][^\\x07\\x1b]*(\\x07|\\x1b\\\\)|[@-Z\\\\-_])"));
    text.remove(escapes);
    text = text.section(QLatin1Char('\n'), -1);
    while (text.endsWith(QLatin1Char('\r'))) {
        text.chop(1);
    }
    text = text.section(QLatin1Char('\r'), -1).trimmed();
    if (text.isEmpty()) {
        return QString();
    }
    // A choice ("[Y/n]", "[J/n]", "[o/N]"), or a sentence ending in "?" or
    // ":" (a password, "Enter a selection (default=all):"). A progress bar
    // ends in "%" or "]" without a "/" in the brackets.
    static const QRegularExpression choice(QStringLiteral("\\[[^\\[\\]/\\s]{1,4}(/[^\\[\\]/\\s]{1,4})+\\]$"));
    if (!choice.match(text).hasMatch() && !text.endsWith(QLatin1Char('?')) && !text.endsWith(QLatin1Char(':'))) {
        return QString();
    }
    if (text.contains(QLatin1String("[sudo]")) || text.contains(QLatin1String("password"), Qt::CaseInsensitive)
        || text.startsWith(QLatin1String("doas ("))) {
        return QStringLiteral("Your password is needed.");
    }
    if (text.startsWith(QLatin1String("::"))) {
        text = text.mid(2).trimmed();
    }
    return text;
}
}
