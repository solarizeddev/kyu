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

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Kyu
{
#ifndef KYU_HAVE_FLATPAK
FlatpakResult checkFlatpak(int)
{
    FlatpakResult result;
    result.available = false;
    return result;
}
#else
namespace
{
// Cancels its GCancellable when the time is up, unless the work finished
// first. libflatpak's network calls stop at the next cancellation check.
class Deadline
{
public:
    explicit Deadline(std::chrono::seconds limit)
        : m_cancellable(g_cancellable_new())
        , m_watch([this, limit]() {
            std::unique_lock lock(m_mutex);
            if (!m_wake.wait_for(lock, limit, [this]() {
                    return m_done;
                })) {
                g_cancellable_cancel(m_cancellable);
            }
        })
    {
    }
    ~Deadline()
    {
        {
            std::lock_guard lock(m_mutex);
            m_done = true;
        }
        m_wake.notify_all();
        m_watch.join();
        g_object_unref(m_cancellable);
    }
    Deadline(const Deadline &) = delete;
    Deadline &operator=(const Deadline &) = delete;
    GCancellable *cancellable() const
    {
        return m_cancellable;
    }
    bool expired() const
    {
        return g_cancellable_is_cancelled(m_cancellable);
    }

private:
    GCancellable *m_cancellable;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_done = false;
    std::thread m_watch; // last: starts once the members above exist
};

QString installationLabel(FlatpakInstallation *installation)
{
    const QString id = QString::fromUtf8(flatpak_installation_get_id(installation));
    if (id == QLatin1String("default")) {
        return QStringLiteral("system");
    }
    return id; // "user", or the id of an extra system installation
}

// Returns false and appends to errors when the installation could not be checked.
bool collect(FlatpakInstallation *installation, QList<FlatpakUpdate> &updates, QStringList &errors, const Deadline &deadline, int timeoutSeconds)
{
    GError *error = nullptr;
    GPtrArray *refs = flatpak_installation_list_installed_refs_for_update(installation, deadline.cancellable(), &error);
    const QString label = installationLabel(installation);
    if (!refs) {
        const QString why = deadline.expired() ? QStringLiteral("no answer within %1 s").arg(timeoutSeconds)
            : error                            ? QString::fromUtf8(error->message)
                                               : QStringLiteral("unknown error");
        errors.append(QStringLiteral("%1 installation: %2").arg(label, why));
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

FlatpakResult checkFlatpak(int timeoutSeconds)
{
    FlatpakResult result;
    QStringList errors;
    const FlatpakWarnings warnings;
    const Deadline deadline{std::chrono::seconds(timeoutSeconds)};

    for (FlatpakInstallation *installation : flatpakInstallations(errors)) {
        if (!deadline.expired()) {
            collect(installation, result.updates, errors, deadline, timeoutSeconds);
        }
        g_object_unref(installation);
    }

    // Out of time: whatever was not checked is not "no updates".
    if (deadline.expired() && errors.isEmpty()) {
        errors.append(QStringLiteral("no answer within %1 s").arg(timeoutSeconds));
    }
    // A remote that could not be read is not "no updates".
    errors.append(warnings.fetchErrors());
    result.ok = errors.isEmpty();
    result.error = errors.join(QLatin1Char('\n'));
    return result;
}
#endif
}
