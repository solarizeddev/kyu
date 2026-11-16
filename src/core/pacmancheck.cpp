#include "pacmancheck.h"

#include "pacmanparse.h"
#include "paths.h"

#include <QDir>

#include <algorithm>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace Kyu
{
namespace
{
QProcessEnvironment cLocaleEnvironment()
{
    auto env = QProcessEnvironment::systemEnvironment();
    // checkupdates and pacman translate their output; parse the C locale.
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    return env;
}

QString lastLines(const QByteArray &text, int count = 5)
{
    QStringList lines = QString::fromUtf8(text).trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() > count) {
        lines = lines.mid(lines.size() - count);
    }
    return lines.join(QLatin1Char('\n'));
}
}

PacmanResult checkPacman(const PacmanCheckOptions &options)
{
    PacmanResult result;
    const QString dbPath = options.dbPath.isEmpty() ? Paths::checkupDbDir() : options.dbPath;
    QDir().mkpath(dbPath);

    auto env = cLocaleEnvironment();
    env.insert(QStringLiteral("CHECKUPDATES_DB"), dbPath);

    QProcess check;
    check.setProcessEnvironment(env);
    check.start(options.checkupdates, {QStringLiteral("--nocolor")});
    if (!check.waitForStarted()) {
        if (QStandardPaths::findExecutable(options.checkupdates).isEmpty() && !QFileInfo(options.checkupdates).isExecutable()) {
            result.error = QStringLiteral("%1 not found; it is part of the pacman-contrib package").arg(options.checkupdates);
        } else {
            result.error = QStringLiteral("%1: %2").arg(options.checkupdates, check.errorString());
        }
        return result;
    }
    if (!check.waitForFinished(options.timeoutMs)) {
        check.kill();
        check.waitForFinished();
        result.error = QStringLiteral("%1 did not finish within %2 s")
                           .arg(options.checkupdates)
                           .arg(options.timeoutMs / 1000);
        return result;
    }

    const QByteArray out = check.readAllStandardOutput();
    const QByteArray err = check.readAllStandardError();
    // checkupdates: 0 = updates listed, 2 = no updates, anything else = failure.
    if (check.exitStatus() != QProcess::NormalExit || (check.exitCode() != 0 && check.exitCode() != 2)) {
        const QString message = lastLines(err);
        result.error = message.isEmpty()
            ? QStringLiteral("%1 exited with code %2").arg(options.checkupdates).arg(check.exitCode())
            : message;
        return result;
    }

    result.ok = true;
    if (check.exitCode() == 2) {
        return result;
    }
    result.packages = PacmanParse::parseCheckupdates(out);
    if (result.packages.isEmpty()) {
        return result;
    }

    QProcess list;
    list.setProcessEnvironment(cLocaleEnvironment());
    list.start(options.pacman, {QStringLiteral("-Sl"), QStringLiteral("--dbpath"), dbPath});
    if (list.waitForFinished(60 * 1000) && list.exitStatus() == QProcess::NormalExit && list.exitCode() == 0) {
        const QByteArray syncList = list.readAllStandardOutput();
        const auto repos = PacmanParse::reposFor(syncList, result.packages);
        for (auto &p : result.packages) {
            p.repo = repos.value(p.name);
        }
        // Group by repo in pacman.conf order; names stay alphabetical within.
        const QStringList order = PacmanParse::repoOrder(syncList);
        auto rank = [&order](const QString &repo) {
            const auto i = order.indexOf(repo);
            return i < 0 ? order.size() : i;
        };
        std::stable_sort(result.packages.begin(), result.packages.end(), [&rank](const PackageUpdate &a, const PackageUpdate &b) {
            return rank(a.repo) < rank(b.repo);
        });
    } else if (list.state() != QProcess::NotRunning) {
        list.kill();
        list.waitForFinished();
    }

    // Sizes from the same database copy. --print needs no root and no lock.
    QProcess sizes;
    sizes.setProcessEnvironment(cLocaleEnvironment());
    sizes.start(options.pacman,
                {QStringLiteral("-Sup"), QStringLiteral("--dbpath"), dbPath, QStringLiteral("--print-format"), QStringLiteral("%n %s")});
    if (sizes.waitForFinished(60 * 1000) && sizes.exitStatus() == QProcess::NormalExit && sizes.exitCode() == 0) {
        const auto bySize = PacmanParse::printedSizes(sizes.readAllStandardOutput());
        for (auto &p : result.packages) {
            p.size = bySize.value(p.name, -1);
        }
    } else if (sizes.state() != QProcess::NotRunning) {
        sizes.kill();
        sizes.waitForFinished();
    }
    return result;
}
}
