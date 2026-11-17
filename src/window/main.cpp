// kyu-window: the update in a window of Kyu's own. Konsole's terminal part
// (the one Dolphin and Kate embed) runs kyu-update; the window has the
// desktop's own decoration, the title "Kyu", and no toolbars or tabs. The
// terminal's colours and font are those of your default Konsole profile.
//
//   kyu-window [kyu-update's options...]
//
// Closing the window while an update runs asks first. Without Konsole's part
// (Konsole not installed), kyu-update opens in Konsole or the terminal
// xdg-terminal-exec picks.
//
// For the screenshot test kit only: KYU_WINDOW_ANSWERS names a pipe whose
// text is typed into the terminal, KYU_WINDOW_ACTIVE_FILE a file that says
// whether the window is the active one ("1" or "0"), KYU_WINDOW_SETTINGS
// changes the profile for this window only (as `konsole -p`, "ColorScheme=
// Linux;..."), and KYU_WINDOW_COLUMNS sets its width in characters.

#include "paths.h"

#include <KParts/ReadOnlyPart>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <kde_terminal_interface.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QLockFile>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>

namespace
{
// kyu-update next to this program (a build tree), else on PATH.
QString updateProgram()
{
    const QString sibling = QCoreApplication::applicationDirPath() + QStringLiteral("/kyu-update");
    if (QFileInfo(sibling).isExecutable()) {
        return sibling;
    }
    const QString found = QStandardPaths::findExecutable(QStringLiteral("kyu-update"));
    return found.isEmpty() ? QStringLiteral("/usr/bin/kyu-update") : found;
}

// kyu-update holds this lock from its start until its last line. Only read
// (taking it, even for a moment, could make a starting kyu-update say
// "already running"), and only this window's kyu-update counts.
bool updateRunning(qint64 terminalPid)
{
    QLockFile lock(Kyu::Paths::stateDir() + QStringLiteral("/update.lock"));
    qint64 pid = 0;
    QString host, app;
    if (!lock.getLockInfo(&pid, &host, &app) || pid <= 0) {
        return false;
    }
    return pid == terminalPid && ::kill(pid_t(pid), 0) == 0;
}

// Without the part: a terminal of the desktop's.
int runInTerminal(const QString &program, const QStringList &args)
{
    if (!QStandardPaths::findExecutable(QStringLiteral("konsole")).isEmpty()
        && QProcess::startDetached(QStringLiteral("konsole"), QStringList{QStringLiteral("-e"), program} + args)) {
        return 0;
    }
    if (!QStandardPaths::findExecutable(QStringLiteral("xdg-terminal-exec")).isEmpty()
        && QProcess::startDetached(QStringLiteral("xdg-terminal-exec"), QStringList{program} + args)) {
        return 0;
    }
    return 1;
}

class Window : public QWidget
{
public:
    Window()
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle(QStringLiteral("Kyu"));
    }
    ~Window() override
    {
        // The part goes with this window's children: not a "finished" then.
        QObject::disconnect(m_partGone);
    }
    // The part, and kyu-update's process id (read once: the part cannot be
    // asked while its session is ending).
    void watch(KParts::ReadOnlyPart *part, qint64 pid)
    {
        m_pid = pid;
        m_partGone = QObject::connect(part, &QObject::destroyed, this, [this]() {
            finished();
        });
    }
    // The program has ended (the part deletes itself then): close without
    // asking, and drop a question about stopping it.
    void finished()
    {
        m_finished = true;
        if (m_box) {
            m_box->reject();
        }
        close();
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (!m_finished && updateRunning(m_pid)) {
            QMessageBox box(this);
            m_box = &box;
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(QStringLiteral("Kyu"));
            box.setText(QStringLiteral("Stop the update?"));
            box.setInformativeText(QStringLiteral("The update is still running. Stopping it now can leave the system half updated."));
            QPushButton *stop = box.addButton(QStringLiteral("Stop the update"), QMessageBox::DestructiveRole);
            QPushButton *keep = box.addButton(QStringLiteral("Keep updating"), QMessageBox::RejectRole);
            box.setDefaultButton(keep);
            box.setEscapeButton(keep);
            box.exec();
            m_box = nullptr;
            if (!m_finished && box.clickedButton() != stop) {
                event->ignore();
                return;
            }
        }
        QObject::disconnect(m_partGone);
        event->accept();
    }
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ActivationChange) {
            const QString path = qEnvironmentVariable("KYU_WINDOW_ACTIVE_FILE");
            if (!path.isEmpty()) {
                QSaveFile file(path);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(isActiveWindow() ? "1" : "0");
                    file.commit();
                }
            }
        }
        return QWidget::event(event);
    }

private:
    bool m_finished = false;
    qint64 m_pid = 0;
    QPointer<QMessageBox> m_box;
    QMetaObject::Connection m_partGone;
};
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Not "konsole": the part shows its tab bar only inside Konsole itself.
    QApplication::setApplicationName(QStringLiteral("kyu-window"));
    QApplication::setApplicationDisplayName(QStringLiteral("Kyu"));
    QApplication::setApplicationVersion(QStringLiteral(KYU_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(KYU_APP_ID));

    const QString program = updateProgram();
    const QStringList args = QCoreApplication::arguments().mid(1);

    auto *window = new Window;
    auto *layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);

    const auto loaded = KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(KPluginMetaData(QStringLiteral("kf6/parts/konsolepart")), window);
    QPointer<KParts::ReadOnlyPart> part = loaded.plugin;
    TerminalInterface *terminal = part ? qobject_cast<TerminalInterface *>(part.data()) : nullptr;
    if (!terminal || !part->widget()) {
        delete window;
        return runInTerminal(program, args);
    }
    layout->addWidget(part->widget());
    window->setFocusProxy(part->widget());


    // Test kit: another colour scheme, for this window only (Konsole's
    // part takes "key=value;..." as konsole's -p does).
    if (const QString settings = qEnvironmentVariable("KYU_WINDOW_SETTINGS"); !settings.isEmpty()) {
        if (!QMetaObject::invokeMethod(part.data(), "changeSessionSettings", Q_ARG(QString, settings))) {
            qWarning("kyu-window: this Konsole part cannot change its settings (KYU_WINDOW_SETTINGS)");
        }
    }

    // About 100 × 34 characters in the profile's font.
    bool columnsSet = false;
    const int columns = qEnvironmentVariableIntValue("KYU_WINDOW_COLUMNS", &columnsSet);
    const int width = columnsSet && columns >= 20 ? columns : 100;
    QSize size(width * 9, 620);
    const QVariant font = terminal->profileProperty(QStringLiteral("Font"));
    if (font.typeId() == QMetaType::QFont) {
        const QFontMetricsF metrics(font.value<QFont>());
        const int scrollBar = window->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
        size = QSize(qCeil(metrics.horizontalAdvance(QLatin1Char('M')) * width) + scrollBar + 8, qCeil(metrics.height() * 34) + 8);
    }
    window->resize(size);
    window->show();

    // Test kit: answers typed into the terminal from a pipe.
    const QByteArray answers = qgetenv("KYU_WINDOW_ANSWERS");
    if (!answers.isEmpty()) {
        const int fd = ::open(answers.constData(), O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, window);
            QObject::connect(notifier, &QSocketNotifier::activated, window, [fd, notifier, part, terminal]() {
                char buffer[256];
                const ssize_t n = ::read(fd, buffer, sizeof buffer);
                if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
                    return;
                }
                if (n <= 0 || !part) {
                    notifier->setEnabled(false);
                    ::close(fd);
                    return;
                }
                QString text = QString::fromUtf8(buffer, int(n));
                text.replace(QLatin1Char('\n'), QLatin1Char('\r')); // Enter
                terminal->sendInput(text);
            });
        }
    }

    // Konsole's session starts in the current directory: the home
    // directory, as in a new terminal (after the test kit's paths above
    // were opened).
    QDir::setCurrent(QDir::homePath());
    // kyu-update leaves out "keep this window open": this window asks.
    qputenv("KYU_WINDOW", "1");
    // Konsole drops the first element of the list (argv[0]) and, with an
    // empty list, starts a shell instead: the program is named twice.
    terminal->startProgram(program, QStringList{program} + args);
    // The part deletes itself when the program ends normally (after
    // kyu-update's last key): the window goes with it.
    window->watch(part.data(), terminal->terminalProcessId());
    part->widget()->setFocus();

    return app.exec();
}
