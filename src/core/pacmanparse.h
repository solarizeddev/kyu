#pragma once

#include "updatestate.h"

#include <QByteArray>
#include <QHash>
#include <QSet>

namespace Kyu::PacmanParse
{
// Parses checkupdates output: one "name oldver -> newver" per line.
// Lines that do not match are returned in *skipped instead of being guessed at.
QList<PackageUpdate> parseCheckupdates(const QByteArray &output, QStringList *skipped = nullptr);

// Parses `pacman -Sl` output ("repo name version [installed...]") and returns
// the repo for each wanted package. If a name is in several repos, the repo
// that carries the wanted version wins, else the first one in pacman.conf order
// (which is the order pacman -Sl prints).
QHash<QString, QString> reposFor(const QByteArray &syncList, const QList<PackageUpdate> &wanted);
}
