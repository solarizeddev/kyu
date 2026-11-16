#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace Kyu
{
struct PackageUpdate {
    QString name;
    QString repo; // empty when the sync databases could not be read
    QString oldVersion;
    QString newVersion;
    qint64 size = -1; // package file size in bytes (pacman's %s); -1 = unknown

    bool operator==(const PackageUpdate &) const = default;
};

struct FlatpakUpdate {
    QString name; // application or runtime id, e.g. org.mozilla.firefox
    QString kind; // "app" or "runtime"
    QString branch;
    QString origin; // remote name, e.g. flathub
    QString installation; // "system", "user" or a named system installation

    bool operator==(const FlatpakUpdate &) const = default;
};

// Result of checking one source (pacman or Flatpak).
struct SourceStatus {
    bool checked = false; // false when the source is disabled or unavailable
    bool ok = false;
    QString error; // the failing tool's own message, when !ok
    QDateTime checkedAt; // last attempt
    QDateTime lastSuccessAt; // last attempt that succeeded

    bool operator==(const SourceStatus &) const = default;
};

struct PacmanResult {
    bool ok = false;
    QString error;
    QList<PackageUpdate> packages;
};

struct FlatpakResult {
    bool available = true; // false when built without libflatpak
    bool ok = false;
    QString error;
    QList<FlatpakUpdate> updates;
};

struct UpdateState {
    static constexpr int FormatVersion = 1;

    QDateTime checkedAt;
    SourceStatus pacman;
    SourceStatus flatpak;
    QList<PackageUpdate> packages;
    QList<FlatpakUpdate> flatpaks;

    int total() const
    {
        return packages.size() + flatpaks.size();
    }
    // Sum of known package sizes; -1 when none is known.
    qint64 totalSize() const;
    bool hasError() const
    {
        return (pacman.checked && !pacman.ok) || (flatpak.checked && !flatpak.ok);
    }

    // Counts per repo (pacman) and per remote (Flatpak), in first-seen order.
    // kyu-check stores packages sorted by repo in pacman.conf order, so repos
    // come out in that order; remotes follow libflatpak's order.
    QList<std::pair<QString, int>> groups() const;

    // "12 updates: core 2 · extra 7 · flathub 3", or "No updates".
    QString summary() const;

    QJsonObject toJson() const;
    static UpdateState fromJson(const QJsonObject &object);

    static std::optional<UpdateState> load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;

    // Builds the new state from fresh results. A source that failed keeps the
    // list from the previous state, so one failed check does not make pending
    // updates disappear; its error is recorded next to it.
    static UpdateState merge(const UpdateState &previous,
                             const std::optional<PacmanResult> &pacman,
                             const std::optional<FlatpakResult> &flatpak,
                             const QDateTime &now);

    bool operator==(const UpdateState &) const = default;
};

QString updatesText(int count);
}
