#pragma once

#include <QStringList>

namespace Kyu
{
// Collects libflatpak's log messages while it lives. libflatpak treats an
// unreachable remote as non-fatal: it logs "Treating remote fetch error as
// non-fatal ..." and reports no updates for that remote. Kyu reports those
// as failures instead of as "no updates".
class FlatpakWarnings
{
public:
    FlatpakWarnings();
    ~FlatpakWarnings();
    FlatpakWarnings(const FlatpakWarnings &) = delete;
    FlatpakWarnings &operator=(const FlatpakWarnings &) = delete;

    // Every message, verbatim.
    QStringList messages() const
    {
        return m_messages;
    }
    // The ones that mean a remote could not be read.
    QStringList fetchErrors() const;

    static bool isFetchError(const QString &message);

private:
    QStringList m_messages;
    unsigned m_handler = 0;
};
}
