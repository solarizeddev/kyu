#include "updatestate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace Kyu
{
namespace
{
QString dateToString(const QDateTime &dt)
{
    return dt.isValid() ? dt.toUTC().toString(Qt::ISODate) : QString();
}

QDateTime dateFromJson(const QJsonValue &value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODate);
}

QJsonObject statusToJson(const SourceStatus &s)
{
    QJsonObject o{
        {QStringLiteral("checked"), s.checked},
        {QStringLiteral("ok"), s.ok},
    };
    if (!s.error.isEmpty()) {
        o.insert(QStringLiteral("error"), s.error);
    }
    if (s.checkedAt.isValid()) {
        o.insert(QStringLiteral("checkedAt"), dateToString(s.checkedAt));
    }
    if (s.lastSuccessAt.isValid()) {
        o.insert(QStringLiteral("lastSuccessAt"), dateToString(s.lastSuccessAt));
    }
    return o;
}

SourceStatus statusFromJson(const QJsonObject &o)
{
    SourceStatus s;
    s.checked = o.value(QStringLiteral("checked")).toBool();
    s.ok = o.value(QStringLiteral("ok")).toBool();
    s.error = o.value(QStringLiteral("error")).toString();
    s.checkedAt = dateFromJson(o.value(QStringLiteral("checkedAt")));
    s.lastSuccessAt = dateFromJson(o.value(QStringLiteral("lastSuccessAt")));
    return s;
}
}

QString updatesText(int count)
{
    // Placeholder until the strings go through KI18n.
    if (count == 0) {
        return QStringLiteral("No updates");
    }
    return count == 1 ? QStringLiteral("1 update") : QStringLiteral("%1 updates").arg(count);
}

QList<std::pair<QString, int>> UpdateState::groups() const
{
    QList<std::pair<QString, int>> result;
    auto add = [&result](const QString &group) {
        const QString key = group.isEmpty() ? QStringLiteral("unknown") : group;
        for (auto &entry : result) {
            if (entry.first == key) {
                ++entry.second;
                return;
            }
        }
        result.append({key, 1});
    };
    for (const auto &p : packages) {
        add(p.repo);
    }
    for (const auto &f : flatpaks) {
        add(f.origin);
    }
    return result;
}

qint64 UpdateState::totalSize() const
{
    qint64 sum = -1;
    for (const auto &p : packages) {
        if (p.size >= 0) {
            sum = qMax<qint64>(sum, 0) + p.size;
        }
    }
    return sum;
}

QString UpdateState::summary() const
{
    QString text = updatesText(total());
    const auto g = groups();
    if (g.isEmpty()) {
        return text;
    }
    QStringList parts;
    for (const auto &[name, count] : g) {
        parts.append(QStringLiteral("%1 %2").arg(name).arg(count));
    }
    return text + QStringLiteral(": ") + parts.join(QStringLiteral(" · "));
}

QJsonObject UpdateState::toJson() const
{
    QJsonArray packageArray;
    for (const auto &p : packages) {
        QJsonObject o{
            {QStringLiteral("name"), p.name},
            {QStringLiteral("repo"), p.repo},
            {QStringLiteral("oldVersion"), p.oldVersion},
            {QStringLiteral("newVersion"), p.newVersion},
        };
        if (p.size >= 0) {
            o.insert(QStringLiteral("size"), p.size);
        }
        if (p.restart) {
            o.insert(QStringLiteral("restart"), true);
        }
        packageArray.append(o);
    }
    QJsonArray flatpakArray;
    for (const auto &f : flatpaks) {
        flatpakArray.append(QJsonObject{
            {QStringLiteral("name"), f.name},
            {QStringLiteral("kind"), f.kind},
            {QStringLiteral("branch"), f.branch},
            {QStringLiteral("origin"), f.origin},
            {QStringLiteral("installation"), f.installation},
        });
    }
    QJsonArray newsItems;
    for (const auto &item : news.items) {
        newsItems.append(QJsonObject{
            {QStringLiteral("title"), item.title},
            {QStringLiteral("link"), item.link},
            {QStringLiteral("published"), dateToString(item.published)},
        });
    }
    QJsonObject newsObject = statusToJson({news.checked, news.ok, news.error, news.checkedAt, {}});
    newsObject.insert(QStringLiteral("source"), news.source);
    newsObject.insert(QStringLiteral("url"), news.url);
    newsObject.insert(QStringLiteral("since"), dateToString(news.since));
    newsObject.insert(QStringLiteral("items"), newsItems);
    return QJsonObject{
        {QStringLiteral("formatVersion"), FormatVersion},
        {QStringLiteral("news"), newsObject},
        {QStringLiteral("checkedAt"), dateToString(checkedAt)},
        {QStringLiteral("pacman"), statusToJson(pacman)},
        {QStringLiteral("flatpak"), statusToJson(flatpak)},
        {QStringLiteral("packages"), packageArray},
        {QStringLiteral("flatpaks"), flatpakArray},
    };
}

UpdateState UpdateState::fromJson(const QJsonObject &object)
{
    UpdateState state;
    state.checkedAt = dateFromJson(object.value(QStringLiteral("checkedAt")));
    state.pacman = statusFromJson(object.value(QStringLiteral("pacman")).toObject());
    state.flatpak = statusFromJson(object.value(QStringLiteral("flatpak")).toObject());
    const auto packageArray = object.value(QStringLiteral("packages")).toArray();
    for (const auto &value : packageArray) {
        const auto o = value.toObject();
        state.packages.append({
            o.value(QStringLiteral("name")).toString(),
            o.value(QStringLiteral("repo")).toString(),
            o.value(QStringLiteral("oldVersion")).toString(),
            o.value(QStringLiteral("newVersion")).toString(),
            o.contains(QStringLiteral("size")) ? o.value(QStringLiteral("size")).toInteger() : -1,
            o.value(QStringLiteral("restart")).toBool(),
        });
    }
    const auto flatpakArray = object.value(QStringLiteral("flatpaks")).toArray();
    for (const auto &value : flatpakArray) {
        const auto o = value.toObject();
        state.flatpaks.append({
            o.value(QStringLiteral("name")).toString(),
            o.value(QStringLiteral("kind")).toString(),
            o.value(QStringLiteral("branch")).toString(),
            o.value(QStringLiteral("origin")).toString(),
            o.value(QStringLiteral("installation")).toString(),
        });
    }
    const QJsonObject n = object.value(QStringLiteral("news")).toObject();
    const SourceStatus ns = statusFromJson(n);
    state.news.checked = ns.checked;
    state.news.ok = ns.ok;
    state.news.error = ns.error;
    state.news.checkedAt = ns.checkedAt;
    state.news.source = n.value(QStringLiteral("source")).toString();
    state.news.url = n.value(QStringLiteral("url")).toString();
    state.news.since = dateFromJson(n.value(QStringLiteral("since")));
    for (const auto &value : n.value(QStringLiteral("items")).toArray()) {
        const auto o = value.toObject();
        state.news.items.append({o.value(QStringLiteral("title")).toString(), o.value(QStringLiteral("link")).toString(),
                                 dateFromJson(o.value(QStringLiteral("published")))});
    }
    return state;
}

std::optional<UpdateState> UpdateState::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!doc.isObject()) {
        if (error) {
            *error = parseError.error != QJsonParseError::NoError ? parseError.errorString()
                                                                  : QStringLiteral("not a JSON object");
        }
        return std::nullopt;
    }
    const int version = doc.object().value(QStringLiteral("formatVersion")).toInt();
    if (version != FormatVersion) {
        if (error) {
            *error = QStringLiteral("unsupported formatVersion %1").arg(version);
        }
        return std::nullopt;
    }
    return fromJson(doc.object());
}

bool UpdateState::save(const QString &path, QString *error) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    // QSaveFile writes to a temporary file and renames it, so a reader never
    // sees a half-written state.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

UpdateState UpdateState::merge(const UpdateState &previous,
                               const std::optional<PacmanResult> &pacman,
                               const std::optional<FlatpakResult> &flatpak,
                               const QDateTime &now,
                               const std::optional<NewsResult> &news)
{
    UpdateState state = previous;
    state.checkedAt = now;

    if (pacman) {
        state.pacman.checked = true;
        state.pacman.ok = pacman->ok;
        state.pacman.checkedAt = now;
        if (pacman->ok) {
            state.pacman.error.clear();
            state.pacman.lastSuccessAt = now;
            state.packages = pacman->packages;
        } else {
            state.pacman.error = pacman->error;
        }
    }

    if (news) {
        state.news.checked = true;
        state.news.ok = news->ok;
        state.news.checkedAt = now;
        state.news.source = news->source.name;
        state.news.url = news->source.url;
        if (news->ok) {
            state.news.error.clear();
            state.news.since = news->since;
            state.news.items = news->items;
        } else {
            state.news.error = news->error;
        }
    }

    if (flatpak) {
        if (!flatpak->available) {
            state.flatpak = SourceStatus{};
            state.flatpaks.clear();
        } else {
            state.flatpak.checked = true;
            state.flatpak.ok = flatpak->ok;
            state.flatpak.checkedAt = now;
            if (flatpak->ok) {
                state.flatpak.error.clear();
                state.flatpak.lastSuccessAt = now;
                state.flatpaks = flatpak->updates;
            } else {
                state.flatpak.error = flatpak->error;
            }
        }
    }
    return state;
}
}
