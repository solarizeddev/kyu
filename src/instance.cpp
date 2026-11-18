// SPDX-License-Identifier: GPL-3.0-or-later
// Manjaro fork: see instance.hpp.
#include "instance.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QThread>

using namespace Qt::StringLiterals;

namespace {

/// In the user's runtime directory (0700): one window per user, and no one
/// else can take the name.
QString serverPath() {
    auto dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    return dir + u'/' + QCoreApplication::applicationName() + u"-instance"_s;
}

/// "app some.AppImage" offers that file, which only a new process can do.
bool opensFile() {
    const auto arguments = QCoreApplication::arguments();
    return arguments.size() > 1 && QFileInfo(arguments.at(1)).isFile();
}

bool send(const QString& page) {
    QLocalSocket socket;
    socket.connectToServer(serverPath());
    if (!socket.waitForConnected(300)) {
        return false;
    }
    socket.write((page + u'\n').toUtf8());
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

QLocalServer* g_server = nullptr;
QPointer<QQmlApplicationEngine> g_engine;
QStringList g_pending;  ///< asked for before the window existed

void present(const QString& page) {
    if (!g_engine || g_engine->rootObjects().isEmpty()) {
        g_pending << page;
        return;
    }
    auto* window = qobject_cast<QQuickWindow*>(g_engine->rootObjects().constFirst());
    if (window == nullptr) {
        return;
    }
    if (page == "updates"_L1) {
        // Not over a page opened on top of a section (an AUR review, say), nor while
        // packages are being changed: then the window only comes forward.
        QQmlExpression free(qmlContext(window), window,
            u"section !== \"updates\" && stack.depth <= 1 && Backend.transaction.state !== Transaction.Running"_s);
        if (free.evaluate().toBool() && !free.hasError()) {
            QMetaObject::invokeMethod(window, "show", Q_ARG(QString, page));
        }
    }
    if (window->visibility() == QWindow::Minimized) {
        window->showNormal();
    }
    window->raise();
    window->requestActivate();
}

void createServer() {
    g_server = new QLocalServer(qApp);
    QObject::connect(g_server, &QLocalServer::newConnection, g_server, [] {
        while (auto* socket = g_server->nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket] {
                if (socket->canReadLine()) {
                    present(QString::fromUtf8(socket->readLine(64)).trimmed());
                }
            });
        }
    });
}

}  // namespace

namespace instance {

QString requestedPage() {
    return QCoreApplication::arguments().contains(u"--updates"_s) ? u"updates"_s : QString();
}

bool forwardToRunning() {
    if (opensFile()) {
        return false;
    }
    if (send(requestedPage())) {
        return true;
    }
    // Binding the name is atomic: of two starts at once, one gets it, the other
    // reaches it. The window loads later; what is asked meanwhile waits for it.
    createServer();
    if (g_server->listen(serverPath())) {
        return false;
    }
    // the other one is starting: give it a moment to answer
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (send(requestedPage())) {
            return true;
        }
        QThread::msleep(300);
    }
    // nobody answers: a name left by a process that crashed
    QLocalServer::removeServer(serverPath());
    g_server->listen(serverPath());
    return false;
}

void serve(QQmlApplicationEngine& engine) {
    if (g_server == nullptr) {
        return;  // a file to open, or the upstream build
    }
    g_engine = &engine;
    auto pages = std::exchange(g_pending, {});
    if (const auto page = requestedPage(); !page.isEmpty()) {
        pages.prepend(page);
    }
    for (const auto& page : std::as_const(pages)) {
        present(page);
    }
}

}  // namespace instance
