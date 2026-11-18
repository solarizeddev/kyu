// SPDX-License-Identifier: GPL-3.0-or-later
// Manjaro fork: see instance.hpp.
#include "instance.hpp"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

using namespace Qt::StringLiterals;

namespace {

/// Per user: QLocalServer puts the name in the user's runtime directory.
QString serverName() {
    return QCoreApplication::applicationName() + u"-instance"_s;
}

/// The pages a start may ask for; anything else only raises the window.
bool knownPage(const QString& page) {
    return page == "updates"_L1;
}

void present(QQmlApplicationEngine& engine, const QString& page) {
    const auto roots = engine.rootObjects();
    if (roots.isEmpty()) {
        return;
    }
    auto* window = qobject_cast<QQuickWindow*>(roots.constFirst());
    if (window == nullptr) {
        return;
    }
    if (knownPage(page)) {
        QMetaObject::invokeMethod(window, "show", Q_ARG(QString, page));
    }
    if (window->visibility() == QWindow::Minimized) {
        window->showNormal();
    }
    window->raise();
    window->requestActivate();
}

}  // namespace

namespace instance {

QString requestedPage() {
    return QCoreApplication::arguments().contains(u"--updates"_s) ? u"updates"_s : QString();
}

bool forwardToRunning() {
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500)) {
        return false;
    }
    socket.write((requestedPage() + u'\n').toUtf8());
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

void serve(QQmlApplicationEngine& engine) {
    if (const auto page = requestedPage(); !page.isEmpty()) {
        present(engine, page);
    }
    auto* server = new QLocalServer(&engine);
    server->setSocketOptions(QLocalServer::UserAccessOption);
    // a server left behind by a crash would refuse the name
    if (!server->listen(serverName())) {
        QLocalServer::removeServer(serverName());
        server->listen(serverName());
    }
    QObject::connect(server, &QLocalServer::newConnection, server, [server, &engine] {
        while (auto* socket = server->nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, &engine] {
                if (!socket->canReadLine()) {
                    return;
                }
                const auto page = QString::fromUtf8(socket->readLine(64)).trimmed();
                present(engine, page);
            });
        }
    });
}

}  // namespace instance
