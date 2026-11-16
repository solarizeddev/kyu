#pragma once

#include "updatestate.h"

#include <QAbstractListModel>

// The package list: repo packages and Flatpaks, grouped by repo or remote
// (the only grouping; see HANDOFF.md rule 2), each with a status that the
// running transaction updates.
class UpdatesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Row pacman is working on, or -1.
    Q_PROPERTY(int workingRow READ workingRow NOTIFY workingRowChanged)
public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        GroupRole,
        KindRole, // "pacman" or "flatpak"
        OldVersionRole,
        NewVersionRole, // Flatpak: branch
        SizeRole,
        StatusRole, // pending, waiting, working, done, failed, unchanged
        ActionRole, // pacman's verb while working/done: upgrading, installing, ...
        RestartRole, // the running kernel's package
    };
    Q_ENUM(Role)

    // Packages pacman changed that the last check did not list (new
    // dependencies, or the repos moved on since the check).
    static QString extraGroup();

    explicit UpdatesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setState(const Kyu::UpdateState &state);

    // Transaction tracking. kind is "pacman" or "flatpak"; pacman runs
    // first, then Flatpak, never both at once.
    void beginTransaction(bool includeFlatpak);
    void includeFlatpak(); // Flatpak part added to a running transaction
    void applyProgress(const QString &action, const QString &package, const QString &kind = QStringLiteral("pacman"));
    void finishKind(const QString &kind, bool success);

    Q_INVOKABLE int countInGroup(const QString &group) const;
    int pacmanCount() const;
    int workingRow() const
    {
        return m_working;
    }
    int flatpakCount() const;
    // Rows pacman changed in the current/last transaction, in order.
    QList<int> changedRows() const;

Q_SIGNALS:
    void countChanged();
    void workingRowChanged();

private:
    struct Row {
        QString name;
        QString group;
        QString kind;
        QString oldVersion;
        QString newVersion;
        qint64 size = -1;
        QString status = QStringLiteral("pending");
        QString action;
        int order = -1; // position in the transaction, -1 if untouched
        bool restart = false;
    };
    static Row makeRow(const QString &name, const QString &group, const QString &kind, const QString &oldVersion, const QString &newVersion, qint64 size);
    void setStatus(int row, const QString &status);

    QList<Row> m_rows;
    int m_working = -1;
    int m_nextOrder = 0;
};
