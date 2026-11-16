#pragma once

#include <QObject>

// Listens for kyu-helper's Event signals on the system bus, so the tray can
// check again after an update even when the panel is closed. (kyu-flatpak
// runs kyu-check itself when it finishes.)
class HelperWatcher : public QObject
{
    Q_OBJECT
public:
    explicit HelperWatcher(QObject *parent = nullptr);

Q_SIGNALS:
    void transactionFinished();

private Q_SLOTS:
    void onEvent(const QString &transaction, qulonglong seq, const QString &record);
};
