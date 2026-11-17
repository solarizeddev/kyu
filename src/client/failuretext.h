#pragma once

#include <QJsonObject>
#include <QStringList>

// What a failed update's "failure" (see docs/helper-protocol.md) means, in
// words, and the command that usually fixes it. Shared by the panel and
// kyu-update. pacman's own lines are shown next to it, verbatim.
struct FailureText {
    QString title;
    QString text;
    QString command; // may be empty
    QString fix; // the fix alone, to run before updating again (kyu-update's [f]); may be empty
    QStringList items; // files, hosts or packages it is about
};

FailureText describeFailure(const QJsonObject &failure);

// Only the title for a failure kind ("keyring" → "Package signatures could
// not be checked"); cheap (runs nothing), for the card.
QString failureTitle(const QString &kind);

// Installed packages whose names end in "-keyring" (archlinux-keyring,
// manjaro-keyring, cachyos-keyring, ...): what to update first.
QStringList installedKeyrings();
// ID from /etc/os-release.
QString distroId();
