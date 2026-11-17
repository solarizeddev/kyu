#pragma once

#include <QStringList>

namespace Kyu
{
// The installed packages that hold pacman's keyrings (archlinux-keyring,
// manjaro-keyring, cachyos-keyring...): those with files in
// /usr/share/pacman/keyrings/, read from pacman's local database. Not
// gnome-keyring. Sorted.
QStringList installedKeyrings(const QString &localDb = QStringLiteral("/var/lib/pacman/local"));
}
