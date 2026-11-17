#pragma once

#include <QJsonObject>
#include <QStringList>

// What a failed update's "failure" (src/core/failure.h kinds) means, in
// words, and the command that usually fixes it. Shared by kyu-update and the
// card. pacman's own lines are shown next to it, verbatim.
struct FailureText {
    QString title;
    QString text;
    QString command; // may be empty
    QString fix; // the fix alone, to run before updating again (kyu-update's [f]); may be empty
    QStringList items; // files, hosts or packages it is about
};

// A "keyring" failure may list the system's keyring packages ("keyrings");
// otherwise they are read from pacman's local database.
FailureText describeFailure(const QJsonObject &failure);

// Only the title for a failure kind ("keyring" → "Package signatures could
// not be checked"); cheap (runs nothing), for the card.
QString failureTitle(const QString &kind);

// ID from /etc/os-release.
QString distroId();
