#pragma once

#include <QAbstractListModel>

// pacman's output, line by line, as the helper relayed it.
class OutputModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Role {
        TextRole = Qt::UserRole + 1,
        StreamRole, // stdout, stderr, or "kyu" for lines Kyu adds (commands, answers)
        LevelRole, // "error", "warning", "note" (Kyu's own lines) or "" (anything else)
    };

    // How a line reads, from its text: tools write progress on stderr too
    // (libflatpak, pacman hooks), so the stream says nothing about trouble.
    static QString levelOf(const QString &text, const QString &stream);

    // Oldest lines are dropped beyond this; the full log stays on disk.
    static constexpr int MaxLines = 20000;

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const QString &text, const QString &stream);
    void clear();
    Q_INVOKABLE QString text() const;
    // The last `count` lines as {line, stream, level} (for the live tail).
    Q_INVOKABLE QVariantList tail(int count) const;

Q_SIGNALS:
    void countChanged();

private:
    QList<std::pair<QString, QString>> m_lines;
};
