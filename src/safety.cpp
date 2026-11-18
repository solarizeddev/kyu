// Safety for Manjaro (see safety.hpp). GPL-3.0-or-later, as the rest.
#include "safety.hpp"

#include "branding.hpp"

#include <QDesktopServices>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace pi::safety {

bool wait_for_pacman_lock(QWidget* parent) {
    while (QFileInfo::exists(QStringLiteral("/var/lib/pacman/db.lck"))) {
        QMessageBox box(QMessageBox::Information, QObject::tr("Another package manager is working"),
            QObject::tr("pamac or pacman is changing the system or refreshing its lists. Try again when it is done. "
                        "If it stays like this after a restart, a lock was left behind: /var/lib/pacman/db.lck."),
            QMessageBox::Retry | QMessageBox::Cancel, parent);
        if (box.exec() != QMessageBox::Retry) {
            return false;
        }
    }
    return true;
}

int pending_updates() {
    QProcess pacman;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    pacman.setProcessEnvironment(env);
    pacman.start(QStringLiteral("pacman"), {QStringLiteral("-Qu")});
    if (!pacman.waitForFinished(30000) || pacman.exitStatus() != QProcess::NormalExit) {
        pacman.kill();
        return -1;
    }
    const QStringList lines = QString::fromUtf8(pacman.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // pacman -Qu exits 1 when nothing is pending.
    if (pacman.exitCode() != 0 && !lines.isEmpty()) {
        return -1;
    }
    return static_cast<int>(std::ranges::count_if(lines, [](const QString& line) { return !line.trimmed().endsWith(QLatin1String("[ignored]")); }));
}

bool ready_to_change(QWidget* parent, bool installing, const std::function<void()>& upgrade_all) {
    if (!wait_for_pacman_lock(parent)) {
        return false;
    }
    const int pending = installing ? pending_updates() : 0;
    if (pending == 0) {
        return true;
    }
    if (pending < 0) {
        return QMessageBox::question(parent, QObject::tr("Updates unknown"),
                   QObject::tr("Whether updates are waiting could not be checked (pacman -Qu failed). Installing while updates wait can break programs. Install anyway?"),
                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
            == QMessageBox::Yes;
    }
    QMessageBox box(QMessageBox::Information, QObject::tr("Update first"),
        (pending == 1 ? QObject::tr("An update is waiting.") : QObject::tr("%1 updates are waiting.").arg(pending)) + QLatin1Char(' ')
            + QObject::tr("Installing now would mix new and old libraries, which can break programs. Update the system first, then install."),
        QMessageBox::Cancel, parent);
    auto* update = box.addButton(QObject::tr("Update Now…"), QMessageBox::AcceptRole);
    box.setDefaultButton(update);
    box.exec();
    if (box.clickedButton() == update && upgrade_all) {
        upgrade_all();
    }
    return false;
}

bool ready_to_upgrade_all(QWidget* parent) {
    if (!wait_for_pacman_lock(parent)) {
        return false;
    }
    const QString notes = QStringLiteral(PI_UPDATE_NOTES_URL);
    if (notes.isEmpty()) {
        return true;
    }
    QMessageBox box(QMessageBox::Information, QObject::tr("Before updating"),
        QObject::tr("%1's announcement for each update says what to do before and after it (a changed setting, "
                    "a package to remove first). Read the newest one before updating.")
            .arg(QStringLiteral(PI_DISTRO_NAME)),
        QMessageBox::Cancel, parent);
    auto* read = box.addButton(QObject::tr("Read the Announcements"), QMessageBox::HelpRole);
    auto* update = box.addButton(QObject::tr("Update Now"), QMessageBox::AcceptRole);
    box.setDefaultButton(read);
    for (;;) {
        box.exec();
        if (box.clickedButton() == read) {
            QDesktopServices::openUrl(QUrl(notes));
            continue;
        }
        return box.clickedButton() == update;
    }
}

bool valid_package_names(const QString& names) {
    static const QRegularExpression name(QStringLiteral("^[a-z0-9@_+][a-z0-9@._+-]*$"));
    const QStringList list = names.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return !list.isEmpty() && std::ranges::all_of(list, [](const QString& n) { return name.match(n).hasMatch(); });
}

}  // namespace pi::safety
