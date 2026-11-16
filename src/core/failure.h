#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>

// Why pacman failed, from its own lines (ported from the old root helper).
// The result is the "failure" object FailureText describes: kind ("network",
// "mirror", "keyring", "files", "disk", "dependencies", "lock"), the lines
// that show it, and what it is about (hosts, files, packages). Empty when the
// lines match no known kind (or pacman spoke another language).
namespace Kyu
{
// A terminal recording (util-linux `script`) as plain lines: carriage-return
// overwrites resolved, escape sequences and backspaces removed.
QStringList terminalLines(const QByteArray &typescript);

// pacman's lines worth keeping as evidence.
bool notableFailureLine(const QString &line);

QJsonObject classifyFailure(const QStringList &lines);
}
