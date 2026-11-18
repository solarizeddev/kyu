#include "launch.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace Kyu
{
QString siblingProgram(const QString &name)
{
    const QString path = QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
    return QFileInfo(path).isExecutable() ? path : name;
}

bool startUpdate(const QStringList &args, const QString &activationToken)
{
    auto start = [&activationToken](const QString &program, const QStringList &arguments) {
        QProcess process;
        process.setProgram(program);
        process.setArguments(arguments);
        if (!activationToken.isEmpty()) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), activationToken);
            process.setProcessEnvironment(env);
        }
        return process.startDetached();
    };
    const QString window = siblingProgram(QStringLiteral("kyu-window"));
    if (!QStandardPaths::findExecutable(window).isEmpty() || QFileInfo(window).isExecutable()) {
        if (start(window, args)) {
            return true;
        }
    }
    const QStringList command = QStringList{siblingProgram(QStringLiteral("kyu-update"))} + args;
    return start(QStringLiteral("konsole"), QStringList{QStringLiteral("-e")} + command) || start(QStringLiteral("xdg-terminal-exec"), command);
}
}
