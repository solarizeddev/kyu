#include "aur.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <cctype>
#include <cstring>
#include <string>

namespace Kyu
{
namespace
{
bool isAlnum(char c)
{
    return std::isalnum(static_cast<unsigned char>(c));
}
bool isAlpha(char c)
{
    return std::isalpha(static_cast<unsigned char>(c));
}
bool isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c));
}

// libalpm's rpmvercmp: segments of digits or letters, separators between.
int rpmvercmp(const std::string &a, const std::string &b)
{
    if (a == b) {
        return 0;
    }
    size_t one = 0, two = 0;
    while (one < a.size() && two < b.size()) {
        const size_t start1 = one, start2 = two;
        while (one < a.size() && !isAlnum(a[one])) {
            ++one;
        }
        while (two < b.size() && !isAlnum(b[two])) {
            ++two;
        }
        if (one >= a.size() || two >= b.size()) {
            break;
        }
        // Different separator lengths: the shorter one is older.
        if (one - start1 != two - start2) {
            return one - start1 < two - start2 ? -1 : 1;
        }
        size_t end1 = one, end2 = two;
        const bool number = isDigit(a[one]);
        if (number) {
            while (end1 < a.size() && isDigit(a[end1])) {
                ++end1;
            }
            while (end2 < b.size() && isDigit(b[end2])) {
                ++end2;
            }
        } else {
            while (end1 < a.size() && isAlpha(a[end1])) {
                ++end1;
            }
            while (end2 < b.size() && isAlpha(b[end2])) {
                ++end2;
            }
        }
        // Segments of different kinds: a number is newer.
        if (two == end2) {
            return number ? 1 : -1;
        }
        std::string s1 = a.substr(one, end1 - one), s2 = b.substr(two, end2 - two);
        if (number) {
            s1.erase(0, std::min(s1.find_first_not_of('0'), s1.size()));
            s2.erase(0, std::min(s2.find_first_not_of('0'), s2.size()));
            if (s1.size() != s2.size()) {
                return s1.size() > s2.size() ? 1 : -1;
            }
        }
        if (const int c = s1.compare(s2); c != 0) {
            return c < 0 ? -1 : 1;
        }
        one = end1;
        two = end2;
    }
    const bool end1 = one >= a.size(), end2 = two >= b.size();
    if (end1 && end2) {
        return 0;
    }
    // "1.0" < "1.0.1", but "1.0a" < "1.0" (a letter suffix is a pre-release).
    if ((end1 && !isAlpha(b[two])) || (!end1 && isAlpha(a[one]))) {
        return -1;
    }
    return 1;
}

struct Evr {
    std::string epoch = "0", version, release;
    bool hasRelease = false;
};

Evr parseEvr(const std::string &evr)
{
    Evr e;
    size_t s = 0;
    while (s < evr.size() && isDigit(evr[s])) {
        ++s;
    }
    const size_t dash = evr.rfind('-');
    size_t versionStart = 0;
    if (s < evr.size() && evr[s] == ':') {
        if (s > 0) {
            e.epoch = evr.substr(0, s);
        }
        versionStart = s + 1;
    }
    if (dash != std::string::npos && dash >= versionStart && dash >= s) {
        e.version = evr.substr(versionStart, dash - versionStart);
        e.release = evr.substr(dash + 1);
        e.hasRelease = true;
    } else {
        e.version = evr.substr(versionStart);
    }
    return e;
}

QByteArray get(const QUrl &url, int timeoutMs, QString *error)
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("kyu-check"));
    request.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs + 1000, &loop, &QEventLoop::quit);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
    }
    QByteArray body;
    if (reply->error() != QNetworkReply::NoError) {
        *error = reply->errorString();
    } else {
        body = reply->readAll();
    }
    reply->deleteLater();
    return body;
}
}

int vercmp(const QString &a, const QString &b)
{
    const std::string sa = a.toStdString(), sb = b.toStdString();
    if (sa == sb) {
        return 0;
    }
    const Evr ea = parseEvr(sa), eb = parseEvr(sb);
    int r = rpmvercmp(ea.epoch, eb.epoch);
    if (r == 0) {
        r = rpmvercmp(ea.version, eb.version);
        if (r == 0 && ea.hasRelease && eb.hasRelease) {
            r = rpmvercmp(ea.release, eb.release);
        }
    }
    return r;
}

std::optional<AurHelper> aurHelperFor(const QString &program)
{
    const QString name = QFileInfo(program).fileName();
    if (name == QLatin1String("paru") || name == QLatin1String("yay")) {
        return AurHelper{name, {program, QStringLiteral("-Sua")}};
    }
    if (name == QLatin1String("pamac")) {
        return AurHelper{name, {program, QStringLiteral("update"), QStringLiteral("--aur")}};
    }
    return std::nullopt;
}

std::optional<AurHelper> findAurHelper(const QString &pamacConf)
{
    for (const QString &name : {QStringLiteral("paru"), QStringLiteral("yay")}) {
        if (const QString program = QStandardPaths::findExecutable(name); !program.isEmpty()) {
            return aurHelperFor(program);
        }
    }
    const QString pamac = QStandardPaths::findExecutable(QStringLiteral("pamac"));
    QFile conf(pamacConf);
    if (pamac.isEmpty() || !conf.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    bool enabled = false, checks = false;
    while (!conf.atEnd()) {
        const QString line = QString::fromUtf8(conf.readLine()).trimmed();
        enabled = enabled || line == QLatin1String("EnableAUR");
        checks = checks || line == QLatin1String("CheckAURUpdates");
    }
    return enabled && checks ? aurHelperFor(pamac) : std::nullopt;
}

std::optional<AurHelper> configuredAurHelper(const QString &configFile, const QString &pamacConf)
{
    QSettings settings(configFile, QSettings::IniFormat);
    if (!settings.value(QStringLiteral("AUR/check"), true).toBool()) {
        return std::nullopt;
    }
    const QString helper = settings.value(QStringLiteral("AUR/helper")).toString();
    if (!helper.isEmpty()) {
        const QString program = QStandardPaths::findExecutable(helper);
        return program.isEmpty() ? std::nullopt : aurHelperFor(program);
    }
    return findAurHelper(pamacConf);
}

std::optional<QHash<QString, QString>> foreignPackages(const QString &pacmanProgram, QString *error)
{
    QProcess pacman;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    pacman.setProcessEnvironment(env);
    pacman.start(pacmanProgram, {QStringLiteral("-Qm")});
    if (!pacman.waitForFinished(30000)) {
        pacman.kill();
        pacman.waitForFinished();
        *error = QStringLiteral("pacman -Qm did not finish");
        return std::nullopt;
    }
    QHash<QString, QString> installed;
    for (const QString &line : QString::fromUtf8(pacman.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            installed.insert(parts.at(0), parts.at(1));
        }
    }
    // pacman -Qm exits 1 when there are none.
    if (installed.isEmpty() && (pacman.exitStatus() != QProcess::NormalExit || (pacman.exitCode() != 0 && pacman.exitCode() != 1))) {
        *error = QStringLiteral("pacman -Qm failed: ") + QString::fromUtf8(pacman.readAllStandardError()).trimmed();
        return std::nullopt;
    }
    return installed;
}

QStringList heldBack(const QString &pacmanConf, const QString &pacman)
{
    const auto lines = [](const QString &program, const QStringList &args) {
        QProcess p;
        p.start(program, args);
        if (!p.waitForFinished(10000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
            p.kill();
            p.waitForFinished();
            return QStringList();
        }
        return QString::fromUtf8(p.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    };
    QStringList held = lines(pacmanConf, {QStringLiteral("IgnorePkg")});

    // One group at a time: pacman fails the lot for one unknown group.
    for (const QString &group : lines(pacmanConf, {QStringLiteral("IgnoreGroup")})) {
        held += lines(pacman, {QStringLiteral("-Qgq"), group});
    }
    return held;
}

AurResult checkAur(const AurCheckOptions &options)
{
    AurResult result;
    const auto installed = foreignPackages(options.pacman, &result.error);
    if (!installed) {
        return result;
    }
    // Held back (IgnorePkg, IgnoreGroup): the helpers skip them too.
    QList<QRegularExpression> held;
    for (const QString &pattern : heldBack(options.pacmanConf, options.pacman)) {
        held.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(pattern)));
    }
    QStringList names;
    for (auto it = installed->cbegin(); it != installed->cend(); ++it) {
        if (std::none_of(held.cbegin(), held.cend(), [&it](const QRegularExpression &re) {
                return re.match(it.key()).hasMatch();
            })) {
            names.append(it.key());
        }
    }
    names.sort();
    if (names.isEmpty()) {
        result.ok = true;
        return result;
    }
    // The AUR's info request, up to 150 names at a time (its URLs stay short).
    for (qsizetype from = 0; from < names.size(); from += 150) {
        QString query;
        for (const QString &name : names.mid(from, 150)) {
            query += (query.isEmpty() ? QString() : QStringLiteral("&")) + QStringLiteral("arg[]=") + QString::fromLatin1(QUrl::toPercentEncoding(name));
        }
        QUrl url(options.rpc);
        url.setQuery(query, QUrl::StrictMode);
        QString error;
        const QByteArray body = get(url, options.timeoutMs, &error);
        if (!error.isEmpty()) {
            result.error = QStringLiteral("%1: %2").arg(options.rpc, error);
            return result;
        }
        const QJsonObject reply = QJsonDocument::fromJson(body).object();
        if (reply.value(QStringLiteral("type")).toString() == QLatin1String("error") || !reply.value(QStringLiteral("results")).isArray()) {
            const QString message = reply.value(QStringLiteral("error")).toString();
            result.error = QStringLiteral("%1: %2").arg(options.rpc, message.isEmpty() ? QStringLiteral("not an AUR reply") : message);
            return result;
        }
        for (const QJsonValue &v : reply.value(QStringLiteral("results")).toArray()) {
            const QJsonObject p = v.toObject();
            const QString name = p.value(QStringLiteral("Name")).toString();
            const QString version = p.value(QStringLiteral("Version")).toString();
            // Only what was asked about (not a held-back one).
            const QString local = names.contains(name) ? installed->value(name) : QString();
            if (!local.isEmpty() && !version.isEmpty() && vercmp(version, local) > 0) {
                result.packages.append({name, QStringLiteral("aur"), local, version, -1, false});
            }
        }
    }
    std::sort(result.packages.begin(), result.packages.end(), [](const PackageUpdate &a, const PackageUpdate &b) {
        return a.name < b.name;
    });
    result.ok = true;
    return result;
}
}
