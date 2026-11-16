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
    case LevelRole:
        return levelOf(line.first, line.second);
    }
    return {};
}

QString OutputModel::levelOf(const QString &text, const QString &stream)
{
    if (stream == QLatin1String("kyu")) {
        return QStringLiteral("note");
    }
    const QString t = text.trimmed();
    if (t.startsWith(QLatin1String("error:"), Qt::CaseInsensitive) || t.startsWith(QLatin1String("==> ERROR"))
        || t.startsWith(QLatin1String("fatal:"), Qt::CaseInsensitive)) {
        return QStringLiteral("error");
    }
    if (t.startsWith(QLatin1String("warning:"), Qt::CaseInsensitive) || t.startsWith(QLatin1String("==> WARNING"))) {
        return QStringLiteral("warning");
    }
    return {};
}

QHash<int, QByteArray> OutputModel::roleNames() const
{
    return {{TextRole, "line"}, {StreamRole, "stream"}, {LevelRole, "level"}};
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

QVariantList OutputModel::tail(int count) const
{
    QVariantList lines;
    for (qsizetype i = qMax<qsizetype>(0, m_lines.size() - count); i < m_lines.size(); ++i) {
        lines.append(QVariantMap{{QStringLiteral("line"), m_lines[i].first},
                                 {QStringLiteral("stream"), m_lines[i].second},
                                 {QStringLiteral("level"), levelOf(m_lines[i].first, m_lines[i].second)}});
    }
    return lines;
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
