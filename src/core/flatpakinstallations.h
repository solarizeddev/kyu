#pragma once

// Include <flatpak.h> before this header.

#include <QList>
#include <QString>
#include <QStringList>

namespace Kyu
{
// The Flatpak installations Kyu checks and updates: the system ones and the
// user one, each only if it has a repository. Without one nothing is
// installed there, and opening it fails with newer flatpak (after asking
// flatpak's root helper to create it), so it is skipped. Problems listing
// the installations go to `errors`. The caller unrefs each installation.
inline QList<FlatpakInstallation *> flatpakInstallations(QStringList &errors)
{
    QList<FlatpakInstallation *> result;
    auto addIfInUse = [&result](FlatpakInstallation *installation) {
        GFile *path = flatpak_installation_get_path(installation);
        GFile *repo = g_file_get_child(path, "repo");
        const bool inUse = g_file_query_exists(repo, nullptr);
        g_object_unref(repo);
        g_object_unref(path);
        if (inUse) {
            result.append(FLATPAK_INSTALLATION(g_object_ref(installation)));
        }
    };
    GError *error = nullptr;
    if (GPtrArray *system = flatpak_get_system_installations(nullptr, &error)) {
        for (guint i = 0; i < system->len; ++i) {
            addIfInUse(FLATPAK_INSTALLATION(g_ptr_array_index(system, i)));
        }
        g_ptr_array_unref(system);
    } else {
        errors.append(QStringLiteral("system installations: %1").arg(error ? QString::fromUtf8(error->message) : QStringLiteral("unknown error")));
        g_clear_error(&error);
    }
    if (FlatpakInstallation *user = flatpak_installation_new_user(nullptr, &error)) {
        addIfInUse(user);
        g_object_unref(user);
    } else {
        errors.append(QStringLiteral("user installation: %1").arg(error ? QString::fromUtf8(error->message) : QStringLiteral("unknown error")));
        g_clear_error(&error);
    }
    return result;
}
}
