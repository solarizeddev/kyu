#ifdef KYU_HAVE_FLATPAK
#include <glib.h>
#endif

#include "flatpakwarnings.h"

namespace Kyu
{
bool FlatpakWarnings::isFetchError(const QString &message)
{
    return message.contains(QLatin1String("remote fetch error"), Qt::CaseInsensitive);
}

QStringList FlatpakWarnings::fetchErrors() const
{
    QStringList errors;
    for (const QString &m : m_messages) {
        if (isFetchError(m)) {
            // "Warning: Treating remote fetch error as non-fatal since X is
            // already installed: <the actual error>"
            const int colon = m.indexOf(QLatin1String("installed: "));
            errors.append(colon >= 0 ? m.mid(colon + 11) : m);
        }
    }
    errors.removeDuplicates();
    return errors;
}

#ifdef KYU_HAVE_FLATPAK
namespace
{
void collect(const gchar *, GLogLevelFlags, const gchar *message, gpointer data)
{
    static_cast<QStringList *>(data)->append(QString::fromUtf8(message));
}
}

FlatpakWarnings::FlatpakWarnings()
{
    m_handler = g_log_set_handler("flatpak",
                                  static_cast<GLogLevelFlags>(G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_INFO),
                                  collect,
                                  &m_messages);
}

FlatpakWarnings::~FlatpakWarnings()
{
    g_log_remove_handler("flatpak", m_handler);
}
#else
FlatpakWarnings::FlatpakWarnings() = default;
FlatpakWarnings::~FlatpakWarnings() = default;
#endif
}
