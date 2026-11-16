#include "updatesmodel.h"

#include <algorithm>

UpdatesModel::UpdatesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QString UpdatesModel::extraGroup()
{
    return QStringLiteral("not in the last check");
}

int UpdatesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant UpdatesModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return r.name;
    case GroupRole:
        return r.group;
    case KindRole:
        return r.kind;
    case OldVersionRole:
        return r.oldVersion;
    case NewVersionRole:
        return r.newVersion;
    case SizeRole:
        return r.size;
    case StatusRole:
        return r.status;
    case ActionRole:
        return r.action;
    case RestartRole:
        return r.restart;
    }
    return {};
}

QHash<int, QByteArray> UpdatesModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {GroupRole, "group"},
        {KindRole, "kind"},
        {OldVersionRole, "oldVersion"},
        {NewVersionRole, "newVersion"},
        {SizeRole, "size"},
        {StatusRole, "status"},
        {ActionRole, "action"},
        {RestartRole, "restart"},
    };
}

UpdatesModel::Row UpdatesModel::makeRow(const QString &name,
                                        const QString &group,
                                        const QString &kind,
                                        const QString &oldVersion,
                                        const QString &newVersion,
                                        qint64 size)
{
    Row row;
    row.name = name;
    row.group = group.isEmpty() ? QStringLiteral("unknown") : group;
    row.kind = kind;
    row.oldVersion = oldVersion;
    row.newVersion = newVersion;
    row.size = size;
    return row;
}

void UpdatesModel::setState(const Kyu::UpdateState &state)
{
    // Groups in first-seen order (pacman.conf order for repos), names in the
    // order the tools printed them (alphabetical for checkupdates).
    const auto groups = state.groups();
    auto groupIndex = [&groups](const QString &group) {
        const QString key = group.isEmpty() ? QStringLiteral("unknown") : group;
        for (int i = 0; i < groups.size(); ++i) {
            if (groups[i].first == key) {
                return i;
            }
        }
        return int(groups.size());
    };

    QList<Row> rows;
    for (const auto &p : state.packages) {
        rows.append(makeRow(p.name, p.repo, QStringLiteral("pacman"), p.oldVersion, p.newVersion, p.size));
        rows.last().restart = p.restart;
    }
    for (const auto &f : state.flatpaks) {
        rows.append(makeRow(f.name, f.origin, QStringLiteral("flatpak"), QString(), f.branch, -1));
    }
    std::stable_sort(rows.begin(), rows.end(), [&](const Row &a, const Row &b) {
        return groupIndex(a.group) < groupIndex(b.group);
    });

    const int oldCount = m_rows.size();
    beginResetModel();
    m_rows = rows;
    m_working = -1;
    m_nextOrder = 0;
    endResetModel();
    if (oldCount != m_rows.size()) {
        Q_EMIT countChanged();
    }
}

void UpdatesModel::setStatus(int row, const QString &status)
{
    if (m_rows[row].status == status) {
        return;
    }
    m_rows[row].status = status;
    const QModelIndex i = index(row);
    Q_EMIT dataChanged(i, i, {StatusRole, ActionRole});
}

void UpdatesModel::beginTransaction(bool includeFlatpak)
{
    m_working = -1;
    m_nextOrder = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        m_rows[i].order = -1;
        m_rows[i].action.clear();
        const bool part = m_rows[i].kind == QLatin1String("pacman") || includeFlatpak;
        setStatus(i, part ? QStringLiteral("waiting") : QStringLiteral("pending"));
    }
}

void UpdatesModel::includeFlatpak()
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].kind == QLatin1String("flatpak") && m_rows[i].status == QLatin1String("pending")) {
            setStatus(i, QStringLiteral("waiting"));
        }
    }
}

void UpdatesModel::applyProgress(const QString &action, const QString &package, const QString &kind)
{
    if (m_working >= 0) {
        setStatus(m_working, QStringLiteral("done"));
    }
    int row = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].kind == kind && m_rows[i].name == package) {
            row = i;
            break;
        }
    }
    if (row < 0) {
        // pacman rows: after the last pacman row, so Flatpaks stay at the
        // end. Flatpak rows (related refs, new runtimes): at the very end.
        int at = 0;
        for (int i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].kind == QLatin1String("pacman") || kind == QLatin1String("flatpak")) {
                at = i + 1;
            }
        }
        beginInsertRows({}, at, at);
        Row added = makeRow(package, extraGroup(), kind, QString(), QString(), -1);
        added.status = QStringLiteral("waiting");
        m_rows.insert(at, added);
        endInsertRows();
        Q_EMIT countChanged();
        row = at;
        if (m_working >= at) {
            ++m_working;
        }
    }
    m_rows[row].action = action;
    m_rows[row].order = m_nextOrder++;
    m_working = row;
    setStatus(row, QStringLiteral("working"));
    Q_EMIT workingRowChanged();
    Q_EMIT dataChanged(index(row), index(row), {ActionRole});
}

void UpdatesModel::finishKind(const QString &kind, bool success)
{
    if (m_working >= 0 && m_rows[m_working].kind == kind) {
        setStatus(m_working, success ? QStringLiteral("done") : QStringLiteral("failed"));
        m_working = -1;
        Q_EMIT workingRowChanged();
    }
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].kind == kind && m_rows[i].status == QLatin1String("waiting")) {
            // Finished without touching it.
            setStatus(i, QStringLiteral("unchanged"));
        }
    }
}

int UpdatesModel::countInGroup(const QString &group) const
{
    return std::count_if(m_rows.cbegin(), m_rows.cend(), [&group](const Row &r) {
        return r.group == group;
    });
}

int UpdatesModel::pacmanCount() const
{
    return std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.kind == QLatin1String("pacman");
    });
}

int UpdatesModel::flatpakCount() const
{
    return m_rows.size() - pacmanCount();
}

QList<int> UpdatesModel::changedRows() const
{
    QList<int> rows;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].order >= 0 && m_rows[i].status == QLatin1String("done")) {
            rows.append(i);
        }
    }
    std::sort(rows.begin(), rows.end(), [this](int a, int b) {
        return m_rows[a].order < m_rows[b].order;
    });
    return rows;
}
