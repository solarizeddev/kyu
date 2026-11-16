// GLib before Qt (see core/flatpakcheck.cpp).
#include <flatpak.h>

#include "flatpakinstallations.h"
#include "flatpakupdater.h"
#include "flatpakwarnings.h"

#include <QJsonArray>

namespace
{
QString fromUtf8(const char *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

// "app/org.mozilla.firefox/x86_64/stable" -> "org.mozilla.firefox"
QString refName(const QString &ref)
{
    return ref.section(QLatin1Char('/'), 1, 1);
}

QString actionFor(FlatpakTransactionOperationType type)
{
    switch (type) {
    case FLATPAK_TRANSACTION_OPERATION_UPDATE:
        return QStringLiteral("updating");
    case FLATPAK_TRANSACTION_OPERATION_INSTALL:
    case FLATPAK_TRANSACTION_OPERATION_INSTALL_BUNDLE:
        return QStringLiteral("installing");
    case FLATPAK_TRANSACTION_OPERATION_UNINSTALL:
        return QStringLiteral("removing");
    default:
        return QStringLiteral("changing");
    }
}

QString installationLabel(FlatpakInstallation *installation)
{
    const QString id = fromUtf8(flatpak_installation_get_id(installation));
    return id == QLatin1String("default") ? QStringLiteral("system") : id;
}

FlatpakUpdater *self(gpointer data)
{
    return static_cast<FlatpakUpdater *>(data);
}

void onNewOperation(FlatpakTransaction *, FlatpakTransactionOperation *op, FlatpakTransactionProgress *, gpointer data)
{
    self(data)->newOperation(op);
}

void onOperationDone(FlatpakTransaction *, FlatpakTransactionOperation *op, const char *, FlatpakTransactionResult, gpointer data)
{
    self(data)->operationDone(op);
}

gboolean onOperationError(FlatpakTransaction *, FlatpakTransactionOperation *op, GError *error, FlatpakTransactionErrorDetails details, gpointer data)
{
    const bool fatal = !(details & FLATPAK_TRANSACTION_ERROR_DETAILS_NON_FATAL);
    return self(data)->operationError(op, error ? fromUtf8(error->message) : QStringLiteral("unknown error"), fatal);
}

gboolean onReady(FlatpakTransaction *transaction, gpointer data)
{
    return self(data)->ready(transaction);
}

gboolean onAddNewRemote(FlatpakTransaction *, FlatpakTransactionRemoteReason, const char *, const char *remote, const char *url, gpointer data)
{
    // Never add remotes on the user's behalf.
    self(data)->output(QStringLiteral("Not adding remote %1 (%2) that the update asks for").arg(fromUtf8(remote), fromUtf8(url)),
                       QStringLiteral("stderr"));
    return FALSE;
}

void onEndOfLife(FlatpakTransaction *, const char *ref, const char *reason, const char *rebase, gpointer data)
{
    self(data)->endOfLife(fromUtf8(ref), fromUtf8(reason), fromUtf8(rebase));
}

gboolean onEndOfLifeWithRebase(FlatpakTransaction *, const char *, const char *ref, const char *reason, const char *rebasedTo, const char **, gpointer data)
{
    // Rebasing installs a different ref: not something to do silently.
    self(data)->endOfLife(fromUtf8(ref), fromUtf8(reason), fromUtf8(rebasedTo));
    return FALSE;
}
}

FlatpakUpdater::FlatpakUpdater(Kyu::JsonLog &log)
    : m_log(log)
{
}

void FlatpakUpdater::output(const QString &text, const QString &stream)
{
    m_log.append(QStringLiteral("output"), {{QStringLiteral("stream"), stream}, {QStringLiteral("text"), text}});
}

void FlatpakUpdater::newOperation(void *operation)
{
    auto *op = static_cast<FlatpakTransactionOperation *>(operation);
    const QString ref = fromUtf8(flatpak_transaction_operation_get_ref(op));
    const QString action = actionFor(flatpak_transaction_operation_get_operation_type(op));
    ++m_current;
    output(QStringLiteral("%1 %2 (%3, from %4)...").arg(action, ref, m_installation, fromUtf8(flatpak_transaction_operation_get_remote(op))));
    m_log.append(QStringLiteral("progress"),
                 {{QStringLiteral("action"), action},
                  {QStringLiteral("package"), refName(ref)},
                  {QStringLiteral("ref"), ref},
                  {QStringLiteral("installation"), m_installation},
                  {QStringLiteral("current"), m_current},
                  {QStringLiteral("total"), qMax(m_result.total, m_current)}});
}

void FlatpakUpdater::operationDone(void *operation)
{
    auto *op = static_cast<FlatpakTransactionOperation *>(operation);
    ++m_result.changed;
    output(QStringLiteral("done: %1").arg(fromUtf8(flatpak_transaction_operation_get_ref(op))));
}

bool FlatpakUpdater::operationError(void *operation, const QString &message, bool fatal)
{
    auto *op = static_cast<FlatpakTransactionOperation *>(operation);
    const QString ref = fromUtf8(flatpak_transaction_operation_get_ref(op));
    const QString line = QStringLiteral("%1: %2").arg(ref, message);
    output(QStringLiteral("error: ") + line, QStringLiteral("stderr"));
    if (fatal) {
        m_result.ok = false;
        m_result.errors.append(line);
    }
    // Go on with the other refs: each ref is deployed atomically.
    return TRUE;
}

bool FlatpakUpdater::ready(void *transaction)
{
    // Now the real operation list is known (it may include related refs
    // such as locales, or new runtimes an update needs).
    GList *ops = flatpak_transaction_get_operations(static_cast<FlatpakTransaction *>(transaction));
    int count = 0;
    for (GList *l = ops; l; l = l->next) {
        if (!flatpak_transaction_operation_get_is_skipped(static_cast<FlatpakTransactionOperation *>(l->data))) {
            ++count;
        }
    }
    g_list_free_full(ops, g_object_unref);
    const int total = qMax(m_estimate, m_doneBefore + count);
    if (total != m_result.total) {
        m_result.total = total;
        m_log.append(QStringLiteral("packages"), {{QStringLiteral("total"), total}});
    }
    m_doneBefore += count;
    return TRUE;
}

void FlatpakUpdater::endOfLife(const QString &ref, const QString &reason, const QString &rebase)
{
    QString text = QStringLiteral("%1 is end-of-life: %2").arg(ref, reason);
    if (!rebase.isEmpty()) {
        text += QStringLiteral(" (replaced by %1; not switched automatically)").arg(rebase);
    }
    output(text, QStringLiteral("stderr"));
}

FlatpakUpdater::Result FlatpakUpdater::run()
{
    // Collect installations with updates first, to know the total.
    struct Pending {
        FlatpakInstallation *installation;
        QStringList refs;
    };
    QList<Pending> pending;
    QList<FlatpakInstallation *> installations;
    const Kyu::FlatpakWarnings warnings;
    qsizetype reported = 0;
    // libflatpak's own messages go into the log, verbatim; unreadable
    // remotes count as failures.
    auto reportWarnings = [&] {
        const QStringList all = warnings.messages();
        for (; reported < all.size(); ++reported) {
            output(all.at(reported), QStringLiteral("stderr"));
        }
    };

    GError *error = nullptr;
    QStringList listErrors;
    installations = Kyu::flatpakInstallations(listErrors);
    if (!listErrors.isEmpty()) {
        m_result.ok = false;
        m_result.errors.append(listErrors);
    }

    for (FlatpakInstallation *installation : std::as_const(installations)) {
        GPtrArray *refs = flatpak_installation_list_installed_refs_for_update(installation, nullptr, &error);
        if (!refs) {
            m_result.ok = false;
            m_result.errors.append(QStringLiteral("%1 installation: %2").arg(installationLabel(installation), fromUtf8(error->message)));
            output(QStringLiteral("error: ") + m_result.errors.constLast(), QStringLiteral("stderr"));
            g_clear_error(&error);
            continue;
        }
        Pending p{installation, {}};
        for (guint i = 0; i < refs->len; ++i) {
            gchar *ref = flatpak_ref_format_ref(FLATPAK_REF(g_ptr_array_index(refs, i)));
            p.refs.append(fromUtf8(ref));
            g_free(ref);
        }
        g_ptr_array_unref(refs);
        if (!p.refs.isEmpty()) {
            m_estimate += p.refs.size();
            pending.append(p);
        }
    }
    reportWarnings();
    for (const QString &fetchError : warnings.fetchErrors()) {
        m_result.ok = false;
        m_result.errors.append(QStringLiteral("could not read a remote: %1").arg(fetchError));
    }
    m_result.total = m_estimate;
    m_log.append(QStringLiteral("packages"), {{QStringLiteral("total"), m_estimate}});

    for (const Pending &p : std::as_const(pending)) {
        m_installation = installationLabel(p.installation);
        output(QStringLiteral("%1 installation: %2").arg(m_installation, p.refs.join(QStringLiteral(", "))));
        FlatpakTransaction *transaction = flatpak_transaction_new_for_installation(p.installation, nullptr, &error);
        if (!transaction) {
            m_result.ok = false;
            m_result.errors.append(QStringLiteral("%1 installation: %2").arg(m_installation, fromUtf8(error->message)));
            g_clear_error(&error);
            continue;
        }
        for (const QString &ref : p.refs) {
            if (!flatpak_transaction_add_update(transaction, ref.toUtf8().constData(), nullptr, nullptr, &error)) {
                m_result.ok = false;
                m_result.errors.append(QStringLiteral("%1: %2").arg(ref, fromUtf8(error->message)));
                g_clear_error(&error);
            }
        }
        g_signal_connect(transaction, "new-operation", G_CALLBACK(onNewOperation), this);
        g_signal_connect(transaction, "operation-done", G_CALLBACK(onOperationDone), this);
        g_signal_connect(transaction, "operation-error", G_CALLBACK(onOperationError), this);
        g_signal_connect(transaction, "ready", G_CALLBACK(onReady), this);
        g_signal_connect(transaction, "add-new-remote", G_CALLBACK(onAddNewRemote), this);
        g_signal_connect(transaction, "end-of-lifed", G_CALLBACK(onEndOfLife), this);
        g_signal_connect(transaction, "end-of-lifed-with-rebase", G_CALLBACK(onEndOfLifeWithRebase), this);

        if (!flatpak_transaction_run(transaction, nullptr, &error)) {
            // Errors already reported per operation end up here too.
            const QString message = fromUtf8(error->message);
            if (!m_result.errors.join(QLatin1Char('\n')).contains(message)) {
                m_result.errors.append(QStringLiteral("%1 installation: %2").arg(m_installation, message));
                output(QStringLiteral("error: ") + m_result.errors.constLast(), QStringLiteral("stderr"));
            }
            m_result.ok = false;
            g_clear_error(&error);
        }
        g_object_unref(transaction);
        reportWarnings();
    }

    for (FlatpakInstallation *installation : std::as_const(installations)) {
        g_object_unref(installation);
    }
    return m_result;
}
