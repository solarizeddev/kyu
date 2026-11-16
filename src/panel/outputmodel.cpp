#include "outputmodel.h"

int OutputModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant OutputModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const auto &line = m_lines.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return line.first;
    case StreamRole:
        return line.second;
    }
    return {};
}

QHash<int, QByteArray> OutputModel::roleNames() const
{
    return {{TextRole, "line"}, {StreamRole, "stream"}};
}

void OutputModel::append(const QString &text, const QString &stream)
{
    if (m_lines.size() >= MaxLines) {
        beginRemoveRows({}, 0, 0);
        m_lines.removeFirst();
        endRemoveRows();
    }
    beginInsertRows({}, m_lines.size(), m_lines.size());
    m_lines.append({text, stream});
    endInsertRows();
    Q_EMIT countChanged();
}

void OutputModel::clear()
{
    beginResetModel();
    m_lines.clear();
    endResetModel();
    Q_EMIT countChanged();
}

QString OutputModel::text() const
{
    QStringList lines;
    lines.reserve(m_lines.size());
    for (const auto &line : m_lines) {
        lines.append(line.first);
    }
    return lines.join(QLatin1Char('\n'));
}
