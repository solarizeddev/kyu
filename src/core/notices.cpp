#include "notices.h"

#include "news.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace Kyu
{
namespace
{
// "a, b and c"
QString listed(const QStringList &items)
{
    if (items.size() < 2) {
        return items.value(0);
    }
    return QStringList(items.mid(0, items.size() - 1)).join(QStringLiteral(", ")) + QStringLiteral(" and ") + items.constLast();
}

bool unreadNews(const TrayInput &in)
{
    for (const NewsItem &item : in.state->news.items) {
        if (!in.newsSeen.contains(newsKey(item))) {
            return true;
        }
    }
    return false;
}
}

QStringList notableUpdates(const UpdateState &state)
{
    // Kernel packages: Arch's linux, linux-lts, linux-zen, linux-hardened;
    // Manjaro's linux612 and the like. Their -headers and modules (linux612-nvidia) are not kernels.
    static const QRegularExpression kernel(QStringLiteral("^linux(\\d+(-rt)?|-lts|-zen|-hardened|-rt|-rt-lts)?$"));
    // nvidia-utils, Manjaro's nvidia-470xx-utils and the like.
    static const QRegularExpression driver(QStringLiteral("^nvidia(-\\d+xx)?-utils$"));
    bool newKernel = false, plasma = false, mesa = false, nvidia = false, systemd = false;
    for (const PackageUpdate &p : state.packages) {
        newKernel = newKernel || p.restart || kernel.match(p.name).hasMatch();
        plasma = plasma || p.name == QLatin1String("plasma-workspace");
        mesa = mesa || p.name == QLatin1String("mesa");
        // The driver itself, not a kernel's module package for it.
        nvidia = nvidia || driver.match(p.name).hasMatch();
        systemd = systemd || p.name == QLatin1String("systemd");
    }
    QStringList names;
    if (newKernel) {
        names.append(QStringLiteral("the kernel"));
    }
    if (plasma) {
        names.append(QStringLiteral("Plasma"));
    }
    if (mesa) {
        names.append(QStringLiteral("Mesa"));
    }
    if (nvidia) {
        names.append(QStringLiteral("the NVIDIA driver"));
    }
    if (systemd) {
        names.append(QStringLiteral("systemd"));
    }
    return names;
}

UpdatesNotice updatesNotice(const TrayInput &in, UpdatesMemory &memory)
{
    UpdatesNotice notice;
    if (in.updating) {
        notice.action = UpdatesNotice::Do::Close;
        return notice;
    }
    // Nothing known: no change (a failed check says nothing about updates;
    // a running one may still add the AUR's).
    if (!in.state || !in.loadError.isEmpty() || in.checking) {
        return notice;
    }
    const UpdateState &s = *in.state;
    const int total = s.total();
    if (total == 0) {
        if (!s.hasError()) {
            memory.told = false;
        }
        notice.action = UpdatesNotice::Do::Close;
        return notice;
    }
    if (memory.told) {
        return notice; // grown or shrunk: the tray shows it
    }
    // A time ahead of now is from a clock that was wrong: not counted.
    const QDateTime now = in.now.toUTC();
    if (memory.sentAt.isValid() && memory.sentAt <= now && memory.sentAt.secsTo(now) < MinHoursBetween * 3600) {
        return notice; // later, if they are still there
    }
    // Unread news has its own notification, which says to read first; this
    // one comes once it is read.
    if (unreadNews(in)) {
        return notice;
    }
    memory.told = true;
    memory.sentAt = now;
    notice.action = UpdatesNotice::Do::Send;
    notice.title = updatesHeading(s);
    notice.text = updatesBreakdown(s);
    if (const QStringList notable = notableUpdates(s); !notable.isEmpty()) {
        notice.text += QStringLiteral(" · including ") + listed(notable);
    }
    return notice;
}

UpdatesMemory readUpdatesMemory(const QString &path)
{
    UpdatesMemory memory;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return memory;
    }
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    memory.told = o.value(QStringLiteral("told")).toBool();
    memory.sentAt = QDateTime::fromString(o.value(QStringLiteral("sentAt")).toString(), Qt::ISODate).toUTC();
    return memory;
}

bool writeUpdatesMemory(const UpdatesMemory &memory, const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QJsonObject o;
    o.insert(QStringLiteral("told"), memory.told);
    if (memory.sentAt.isValid()) {
        o.insert(QStringLiteral("sentAt"), memory.sentAt.toUTC().toString(Qt::ISODateWithMs));
    }
    file.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return file.commit();
}
}
