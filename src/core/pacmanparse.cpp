#include "pacmanparse.h"

namespace Kyu::PacmanParse
{
QList<PackageUpdate> parseCheckupdates(const QByteArray &output, QStringList *skipped)
{
    QList<PackageUpdate> result;
    const auto lines = output.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        // checkupdates filters "[ignored]" lines itself; anything with a
        // fourth field is not a plain update line, so leave it out.
        if (fields.size() != 4 || fields.at(2) != QLatin1String("->")) {
            if (skipped) {
                skipped->append(line);
            }
            continue;
        }
        result.append({fields.at(0), QString(), fields.at(1), fields.at(3)});
    }
    return result;
}

QStringList repoOrder(const QByteArray &syncList)
{
    QStringList repos;
    const auto lines = syncList.split('\n');
    for (const QByteArray &rawLine : lines) {
        const int space = rawLine.indexOf(' ');
        if (space <= 0) {
            continue;
        }
        const QString repo = QString::fromUtf8(rawLine.left(space));
        if (repos.isEmpty() || repos.constLast() != repo) {
            if (!repos.contains(repo)) {
                repos.append(repo);
            }
        }
    }
    return repos;
}

QHash<QString, qint64> printedSizes(const QByteArray &output)
{
    QHash<QString, qint64> sizes;
    const auto lines = output.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QList<QByteArray> fields = rawLine.trimmed().split(' ');
        if (fields.size() != 2) {
            continue;
        }
        bool ok = false;
        const qint64 size = fields.at(1).toLongLong(&ok);
        if (ok && size >= 0) {
            sizes.insert(QString::fromUtf8(fields.at(0)), size);
        }
    }
    return sizes;
}

QHash<QString, QString> reposFor(const QByteArray &syncList, const QList<PackageUpdate> &wanted)
{
    QHash<QString, QString> wantedVersion;
    for (const auto &p : wanted) {
        wantedVersion.insert(p.name, p.newVersion);
    }

    QHash<QString, QString> firstRepo;
    QHash<QString, QString> exactRepo;
    const auto lines = syncList.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QList<QByteArray> fields = rawLine.simplified().split(' ');
        if (fields.size() < 3) {
            continue;
        }
        const QString name = QString::fromUtf8(fields.at(1));
        const auto it = wantedVersion.constFind(name);
        if (it == wantedVersion.constEnd()) {
            continue;
        }
        const QString repo = QString::fromUtf8(fields.at(0));
        if (!firstRepo.contains(name)) {
            firstRepo.insert(name, repo);
        }
        if (!exactRepo.contains(name) && QString::fromUtf8(fields.at(2)) == it.value()) {
            exactRepo.insert(name, repo);
        }
    }
    for (auto it = exactRepo.constBegin(); it != exactRepo.constEnd(); ++it) {
        firstRepo.insert(it.key(), it.value());
    }
    return firstRepo;
}
}
