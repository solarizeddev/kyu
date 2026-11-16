// libflatpak pulls in GLib headers, which must come before any Qt header
// (GLib uses "signals" as an identifier; the project builds with
// QT_NO_KEYWORDS, but keep the order anyway).
#ifdef KYU_HAVE_FLATPAK
#include <flatpak.h>
#endif

#include "flatpakcheck.h"
#ifdef KYU_HAVE_FLATPAK
#include "flatpakinstallations.h"
#endif
#include "flatpakwarnings.h"

#include <QStringList>

namespace Kyu
{
#ifndef KYU_HAVE_FLATPAK
FlatpakResult checkFlatpak()
{
    FlatpakResult result;
    result.available = false;
    return result;
}
#else
namespace
{
QString installationLabel(FlatpakInstallation *installation)
{
    const QString id = QString::fromUtf8(flatpak_installation_get_id(installation));
    if (id == QLatin1String("default")) {
        return QStringLiteral("system");
    }
    return id; // "user", or the id of an extra system installation
}

// Returns false and appends to errors when the installation could not be checked.
bool collect(FlatpakInstallation *installation, QList<FlatpakUpdate> &updates, QStringList &errors)
{
    GError *error = nullptr;
    GPtrArray *refs = flatpak_installation_list_installed_refs_for_update(installation, nullptr, &error);
    const QString label = installationLabel(installation);
    if (!refs) {
        errors.append(QStringLiteral("%1 installation: %2")
                          .arg(label, error ? QString::fromUtf8(error->message) : QStringLiteral("unknown error")));
        g_clear_error(&error);
        return false;
    }
    for (guint i = 0; i < refs->len; ++i) {
        auto *installed = FLATPAK_INSTALLED_REF(g_ptr_array_index(refs, i));
        auto *ref = FLATPAK_REF(installed);
        updates.append({
            QString::fromUtf8(flatpak_ref_get_name(ref)),
            flatpak_ref_get_kind(ref) == FLATPAK_REF_KIND_APP ? QStringLiteral("app") : QStringLiteral("runtime"),
            QString::fromUtf8(flatpak_ref_get_branch(ref)),
            QString::fromUtf8(flatpak_installed_ref_get_origin(installed)),
            label,
        });
    }
    g_ptr_array_unref(refs);
    return true;
}
}

FlatpakResult checkFlatpak()
{
    FlatpakResult result;
    QStringList errors;
    const FlatpakWarnings warnings;

    for (FlatpakInstallation *installation : flatpakInstallations(errors)) {
        collect(installation, result.updates, errors);
        g_object_unref(installation);
    }

    // A remote that could not be read is not "no updates".
    errors.append(warnings.fetchErrors());
    result.ok = errors.isEmpty();
    result.error = errors.join(QLatin1Char('\n'));
    return result;
}
#endif
}
