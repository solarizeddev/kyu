#include "testdriver.h"

#include "panelcontroller.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

namespace TestDriver
{
namespace
{
QCommandLineOption screenshot(QStringLiteral("screenshot"), QStringLiteral("Testing: save the window to FILE after --delay and quit."), QStringLiteral("file"));
QCommandLineOption delay(QStringLiteral("delay"), QStringLiteral("Testing: wait MS before the screenshot."), QStringLiteral("ms"), QStringLiteral("1500"));
QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Testing: window size WxH in logical pixels."), QStringLiteral("WxH"));
QCommandLineOption page(QStringLiteral("page"),
                        QStringLiteral("Testing: open \"output\", \"changes\", \"configs\", \"diff\" (the first settings file) or \"processes\" after --page-delay."),
                        QStringLiteral("name"));
QCommandLineOption pageDelay(QStringLiteral("page-delay"), QStringLiteral("Testing: delay for --page."), QStringLiteral("ms"), QStringLiteral("500"));
QCommandLineOption startUpdate(QStringLiteral("start-update"), QStringLiteral("Testing, with --session-bus only: press Update all after loading."));
QCommandLineOption answer(QStringLiteral("answer"), QStringLiteral("Testing, with --session-bus only: answer pacman's question with VALUE (\"cancel\" cancels)."),
                          QStringLiteral("value"));
QCommandLineOption press(QStringLiteral("press"),
                         QStringLiteral("Testing, with --session-bus only: press KEY (Qt key name, e.g. Return, L, Escape) after MS: KEY@MS, repeatable."),
                         QStringLiteral("key@ms"));
}

void addOptions(QCommandLineParser &parser)
{
    parser.addOptions({screenshot, delay, size, page, pageDelay, startUpdate, answer, press});
}

void run(QQmlApplicationEngine &engine, const QCommandLineParser &parser, PanelController &panel, bool sessionBus)
{
    QObject *root = engine.rootObjects().constFirst();
    if (parser.isSet(size)) {
        const QStringList wh = parser.value(size).split(QLatin1Char('x'));
        if (wh.size() == 2) {
            root->setProperty("width", wh[0].toInt());
            root->setProperty("height", wh[1].toInt());
        }
    }
    // Acting options only against a test helper on the session bus: they
    // must never start or answer a real system update.
    const bool acting = parser.isSet(startUpdate) || parser.isSet(answer) || parser.isSet(press);
    if (acting && !sessionBus) {
        qWarning("kyu: --start-update, --answer and --press need --session-bus (a test helper); ignored");
    }
    if (parser.isSet(startUpdate) && sessionBus) {
        QTimer::singleShot(300, &panel, &PanelController::updateAll);
    }
    if (parser.isSet(answer) && sessionBus) {
        // As if the user picked it and pressed Continue (or Cancel).
        const QString value = parser.value(answer);
        QObject::connect(&panel, &PanelController::changed, &panel, [&panel, value] {
            if (panel.screen() == QLatin1String("question") && !panel.replying()) {
                QTimer::singleShot(300, &panel, [&panel, value] {
                    if (value == QLatin1String("cancel")) {
                        panel.cancelUpdate();
                    } else {
                        panel.answer(value);
                    }
                });
            }
        });
    }
    for (const QString &spec : sessionBus ? parser.values(press) : QStringList()) {
        const QString keyName = spec.section(QLatin1Char('@'), 0, 0);
        const int at = spec.section(QLatin1Char('@'), 1).toInt();
        QTimer::singleShot(at, root, [root, keyName] {
            auto *window = qobject_cast<QQuickWindow *>(root);
            const QKeySequence sequence(keyName);
            if (!window || sequence.isEmpty()) {
                qWarning("kyu: cannot press %s", qPrintable(keyName));
                return;
            }
            window->requestActivate();
            const int key = sequence[0].key();
            const QString text = keyName.size() == 1 ? keyName.toLower() : QString();
            QKeyEvent down(QEvent::KeyPress, key, Qt::NoModifier, text);
            QKeyEvent up(QEvent::KeyRelease, key, Qt::NoModifier, text);
            QCoreApplication::sendEvent(window, &down);
            QCoreApplication::sendEvent(window, &up);
        });
    }
    if (parser.isSet(page)) {
        const QString name = parser.value(page);
        QTimer::singleShot(parser.value(pageDelay).toInt(), root, [root, name] {
            if (name == QLatin1String("diff")) {
                QObject *panel = qvariant_cast<QObject *>(qmlContext(root)->contextProperty(QStringLiteral("Panel")));
                const QVariantList configs = panel ? panel->property("configs").toList() : QVariantList();
                if (!configs.isEmpty()) {
                    const QVariantMap first = configs.first().toMap();
                    QMetaObject::invokeMethod(root, "showDiff", Q_ARG(QVariant, first.value(QStringLiteral("path"))),
                                              Q_ARG(QVariant, first.value(QStringLiteral("saved"))));
                }
                return;
            }
            const char *method = name == QLatin1String("changes")     ? "showChanges"
                               : name == QLatin1String("configs")     ? "showConfigs"
                               : name == QLatin1String("processes")   ? "showProcesses"
                                                                       : "showOutput";
            QMetaObject::invokeMethod(root, method);
        });
    }
    if (parser.isSet(screenshot)) {
        const QString file = parser.value(screenshot);
        QTimer::singleShot(parser.value(delay).toInt(), root, [root, file] {
            auto *window = qobject_cast<QQuickWindow *>(root);
            const bool ok = window && window->grabWindow().save(file);
            if (!ok) {
                qWarning("kyu: could not save %s", qPrintable(file));
            }
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
}
}
