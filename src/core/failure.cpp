#include "failure.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace Kyu
{
namespace
{
QJsonArray unique(const QStringList &items)
{
    QJsonArray out;
    QStringList seen;
    for (const QString &i : items) {
        if (!i.isEmpty() && !seen.contains(i)) {
            seen.append(i);
            out.append(i);
        }
    }
    return out;
}

QJsonArray first(const QStringList &lines, int count = 20)
{
    QJsonArray out;
    for (const QString &l : lines.mid(0, count)) {
        out.append(l);
    }
    return out;
}

// "error: failed retrieving file 'NAME' from HOST : REASON"
QString quoted(const QString &line)
{
    const int start = line.indexOf(QLatin1Char('\''));
    const int end = start < 0 ? -1 : line.indexOf(QLatin1Char('\''), start + 1);
    return end < 0 ? QString() : line.mid(start + 1, end - start - 1);
}

QString host(const QString &line)
{
    const int at = line.indexOf(QLatin1String("' from "));
    if (at < 0) {
        return {};
    }
    return line.mid(at + 7).section(QStringLiteral(" : "), 0, 0).trimmed();
}

QString reason(const QString &line)
{
    return line.section(QStringLiteral(" : "), -1);
}

bool networkReason(const QString &line)
{
    const QString r = reason(line);
    for (const char *n : {"Could not resolve host", "Couldn't resolve host", "Failed to connect", "Could not connect", "Connection timed out",
                          "Connection refused", "Network is unreachable", "Operation too slow", "Resolving timed out", "Connection reset"}) {
        if (r.contains(QLatin1String(n))) {
            return true;
        }
    }
    return false;
}

// "pkg: /usr/bin/x exists in filesystem", "... (owned by other)", or
// "/usr/bin/x exists in both 'a' and 'b'": what is before " exists".
const QRegularExpression &conflictLine()
{
    static const QRegularExpression re(QStringLiteral("^(.*) exists in (?:filesystem(?: \\(owned by .*\\))?|both '.*' and '.*')$"));
    return re;
}

bool missingOnMirror(const QString &line)
{
    const QString r = reason(line);
    return r.contains(QLatin1String("returned error: 404")) || r.contains(QLatin1String("returned error: 410"));
}
}

QStringList terminalLines(const QByteArray &typescript)
{
    QString text = QString::fromUtf8(typescript);
    // Escape sequences: CSI (ESC [ ... final byte) and OSC (ESC ] ... BEL/ST).
    static const QRegularExpression csi(QStringLiteral("\\x1b\\[[0-9;?]*[ -/]*[@-~]"));
    static const QRegularExpression osc(QStringLiteral("\\x1b\\][^\\x07\\x1b]*(?:\\x07|\\x1b\\\\)"));
    text.remove(osc);
    text.remove(csi);
    QStringList lines;
    for (QString line : text.split(QLatin1Char('\n'))) {
        // A carriage return starts the line over (progress bars): keep what was written last.
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        const int cr = line.lastIndexOf(QLatin1Char('\r'));
        if (cr >= 0) {
            line = line.mid(cr + 1);
        }
        QString clean;
        for (const QChar c : line) {
            if (c == QLatin1Char('\b')) {
                clean.chop(1);
            } else if (c.unicode() >= 0x20 || c == QLatin1Char('\t')) {
                clean.append(c);
            }
        }
        lines.append(clean);
    }
    return lines;
}

bool notableFailureLine(const QString &line)
{
    const QString l = line.trimmed();
    return l.startsWith(QLatin1String("error:")) || l.startsWith(QLatin1String("warning: failed")) || conflictLine().match(l).hasMatch()
        || l.contains(QLatin1String("breaks dependency")) || l.contains(QLatin1String("unable to satisfy dependency"))
        || l.startsWith(QLatin1String("Errors occurred"));
}

QJsonObject classifyFailure(const QStringList &allLines)
{
    QStringList lines;
    for (const QString &l : allLines) {
        if (lines.size() < 200 && notableFailureLine(l)) {
            lines.append(l.trimmed());
        }
    }
    auto has = [&](const char *needle) {
        for (const QString &l : lines) {
            if (l.contains(QLatin1String(needle))) {
                return true;
            }
        }
        return false;
    };
    auto matching = [&](const std::function<bool(const QString &)> &pred) {
        QStringList out;
        for (const QString &l : lines) {
            if (pred(l)) {
                out.append(l);
            }
        }
        return first(out);
    };
    QStringList retrieving;
    for (const QString &l : lines) {
        if (l.contains(QLatin1String("failed retrieving file"))) {
            retrieving.append(l);
        }
    }

    QJsonObject f;
    if (has("unable to lock database") || has("could not lock database")) {
        f = {{QStringLiteral("kind"), QStringLiteral("lock")}, {QStringLiteral("lines"), matching([](const QString &l) { return l.contains(QLatin1String("lock")); })}};
    } else if (!retrieving.isEmpty() && std::all_of(retrieving.cbegin(), retrieving.cend(), networkReason)) {
        QStringList hosts;
        for (const QString &l : retrieving) {
            hosts.append(host(l));
        }
        f = {{QStringLiteral("kind"), QStringLiteral("network")}, {QStringLiteral("lines"), first(retrieving)}, {QStringLiteral("hosts"), unique(hosts)}};
    } else if (std::any_of(retrieving.cbegin(), retrieving.cend(), missingOnMirror)) {
        QStringList files, hosts;
        for (const QString &l : retrieving) {
            if (missingOnMirror(l)) {
                files.append(quoted(l));
                hosts.append(host(l));
            }
        }
        f = {{QStringLiteral("kind"), QStringLiteral("mirror")}, {QStringLiteral("lines"), first(retrieving)},
             {QStringLiteral("files"), unique(files)}, {QStringLiteral("hosts"), unique(hosts)}};
    } else if (has("unknown trust") || has("marginal trust") || has("(PGP signature)") || has("required key missing from keyring")
               || has("could not be looked up remotely") || (has("signature from") && has("is invalid"))) {
        QStringList packages;
        for (const QString &l : lines) {
            if (l.contains(QLatin1String("signature from")) && l.startsWith(QLatin1String("error: "))) {
                packages.append(l.mid(7).section(QLatin1Char(':'), 0, 0));
            }
        }
        f = {{QStringLiteral("kind"), QStringLiteral("keyring")},
             {QStringLiteral("lines"), matching([](const QString &l) {
                  return l.contains(QLatin1String("signature")) || l.contains(QLatin1String("key")) || l.contains(QLatin1String("PGP"));
              })},
             {QStringLiteral("packages"), unique(packages)}};
    } else if (has("conflicting files")) {
        QJsonArray files;
        int count = 0;
        for (const QString &l : lines) {
            if (const auto m = conflictLine().match(l); m.hasMatch()) {
                if (++count <= 50) {
                    files.append(m.captured(1));
                }
            }
        }
        f = {{QStringLiteral("kind"), QStringLiteral("files")},
             {QStringLiteral("lines"), matching([](const QString &l) {
                  return l.contains(QLatin1String("conflicting files")) || l.startsWith(QLatin1String("Errors occurred"));
              })},
             {QStringLiteral("files"), files}, {QStringLiteral("fileCount"), count}};
    } else if (has("too full") || has("not enough free disk space")) {
        f = {{QStringLiteral("kind"), QStringLiteral("disk")},
             {QStringLiteral("lines"), matching([](const QString &l) { return l.contains(QLatin1String("too full")) || l.contains(QLatin1String("disk space")); })}};
    } else if (has("could not satisfy dependencies")) {
        f = {{QStringLiteral("kind"), QStringLiteral("dependencies")},
             {QStringLiteral("lines"), matching([](const QString &l) { return l.contains(QLatin1String("dependenc")) || l.startsWith(QLatin1String("error:")); })}};
    }
    return f;
}
}
