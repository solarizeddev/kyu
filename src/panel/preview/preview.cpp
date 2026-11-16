#include "preview.h"

#include "panelcontroller.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQmlContext>
#include <QQuickWindow>
#include <QKeyEvent>
#include <QKeySequence>
#include <QTimer>
#include <QUrlQuery>
#include <QtQml/qqmlregistration.h>

namespace
{
bool g_dark = false;

// Breeze / Breeze Dark values, standing in for the user's colour scheme.
struct Set {
    const char *bg, *alt, *text, *disabled, *link, *neutralBg;
};
const Set LightWindow{"#eff0f1", "#e3e5e7", "#232629", "#707d8a", "#2980b9", "#fbe6cf"};
const Set LightView{"#ffffff", "#f7f7f7", "#232629", "#707d8a", "#2980b9", "#fbe6cf"};
const Set DarkWindow{"#202326", "#292c30", "#fcfcfc", "#a1a9b1", "#1d99f3", "#4b3a26"};
const Set DarkView{"#141618", "#1d1f22", "#fcfcfc", "#a1a9b1", "#1d99f3", "#4b3a26"};
const Set Complementary{"#2a2e32", "#1b1e20", "#fcfcfc", "#a1a9b1", "#1d99f3", "#4b3a26"};
}

class StubTheme : public QObject
{
    Q_OBJECT
    QML_ATTACHED(StubTheme)
    Q_PROPERTY(ColorSet colorSet READ colorSet WRITE setColorSet NOTIFY changed)
    Q_PROPERTY(bool inherit MEMBER m_inherit NOTIFY changed)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY changed)
    Q_PROPERTY(QColor disabledTextColor READ disabledTextColor NOTIFY changed)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY changed)
    Q_PROPERTY(QColor alternateBackgroundColor READ alternateBackgroundColor NOTIFY changed)
    Q_PROPERTY(QColor highlightColor READ highlightColor NOTIFY changed)
    Q_PROPERTY(QColor highlightedTextColor READ highlightedTextColor NOTIFY changed)
    Q_PROPERTY(QColor linkColor READ linkColor NOTIFY changed)
    Q_PROPERTY(QColor positiveTextColor READ positiveTextColor NOTIFY changed)
    Q_PROPERTY(QColor neutralTextColor READ neutralTextColor NOTIFY changed)
    Q_PROPERTY(QColor negativeTextColor READ negativeTextColor NOTIFY changed)
    Q_PROPERTY(QColor neutralBackgroundColor READ neutralBackgroundColor NOTIFY changed)
    Q_PROPERTY(QFont defaultFont READ defaultFont CONSTANT)
    Q_PROPERTY(QFont smallFont READ smallFont CONSTANT)
    Q_PROPERTY(QFont fixedWidthFont READ fixedWidthFont CONSTANT)
public:
    enum ColorSet { View, Window, Button, Selection, Tooltip, Complementary, Header };
    Q_ENUM(ColorSet)

    explicit StubTheme(QObject *parent)
        : QObject(parent)
    {
        connect(broadcaster(), &QObject::objectNameChanged, this, &StubTheme::changed);
    }
    static StubTheme *qmlAttachedProperties(QObject *object)
    {
        return new StubTheme(object);
    }

    ColorSet colorSet() const
    {
        if (m_explicit) {
            return m_set;
        }
        // Inherit from the nearest visual ancestor that set one, like Kirigami.
        QObject *p = parent();
        while ((p = next(p))) {
            if (auto *t = qobject_cast<StubTheme *>(qmlAttachedPropertiesObject<StubTheme>(p, false)); t && t->m_explicit) {
                return t->m_set;
            }
        }
        return Window;
    }
    void setColorSet(ColorSet set)
    {
        m_set = set;
        m_explicit = true;
        // Descendants may have evaluated before this was set.
        broadcaster()->setObjectName(QString::number(broadcaster()->objectName().toInt() + 1));
    }

    QColor textColor() const { return QColor(QLatin1String(set().text)); }
    QColor disabledTextColor() const { return QColor(QLatin1String(set().disabled)); }
    QColor backgroundColor() const { return QColor(QLatin1String(set().bg)); }
    QColor alternateBackgroundColor() const { return QColor(QLatin1String(set().alt)); }
    QColor highlightColor() const { return QColor(QStringLiteral("#3daee9")); }
    QColor highlightedTextColor() const { return QColor(QStringLiteral("#ffffff")); }
    QColor linkColor() const { return QColor(QLatin1String(set().link)); }
    QColor positiveTextColor() const { return QColor(QStringLiteral("#27ae60")); }
    QColor neutralTextColor() const { return QColor(QStringLiteral("#f67400")); }
    QColor negativeTextColor() const { return QColor(QStringLiteral("#da4453")); }
    QColor neutralBackgroundColor() const { return QColor(QLatin1String(set().neutralBg)); }
    QFont defaultFont() const { return QApplication::font(); }
    QFont smallFont() const
    {
        QFont f = QApplication::font();
        f.setPointSizeF(f.pointSizeF() * 0.9);
        return f;
    }
    QFont fixedWidthFont() const { return QFontDatabase::systemFont(QFontDatabase::FixedFont); }

Q_SIGNALS:
    void changed();

private:
    static QObject *next(QObject *o)
    {
        if (auto *item = qobject_cast<QQuickItem *>(o); item && item->parentItem()) {
            return item->parentItem();
        }
        return o ? o->parent() : nullptr;
    }
    static QObject *broadcaster()
    {
        static QObject o;
        return &o;
    }
    const Set &set() const
    {
        switch (colorSet()) {
        case Complementary:
            return ::Complementary;
        case View:
            return g_dark ? DarkView : LightView;
        default:
            return g_dark ? DarkWindow : LightWindow;
        }
    }
    ColorSet m_set = Window;
    bool m_explicit = false;
    bool m_inherit = true;
};

class StubIconSizes : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int small MEMBER m_small CONSTANT)
    Q_PROPERTY(int smallMedium MEMBER m_smallMedium CONSTANT)
    Q_PROPERTY(int medium MEMBER m_medium CONSTANT)
    Q_PROPERTY(int large MEMBER m_large CONSTANT)
    Q_PROPERTY(int huge MEMBER m_huge CONSTANT)
public:
    using QObject::QObject;

private:
    int m_small = 16, m_smallMedium = 22, m_medium = 32, m_large = 48, m_huge = 64;
};

class StubUnits : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int gridUnit READ gridUnit CONSTANT)
    Q_PROPERTY(int smallSpacing MEMBER m_small CONSTANT)
    Q_PROPERTY(int mediumSpacing MEMBER m_medium CONSTANT)
    Q_PROPERTY(int largeSpacing MEMBER m_large CONSTANT)
    Q_PROPERTY(int longDuration MEMBER m_long CONSTANT)
    Q_PROPERTY(QObject *iconSizes MEMBER m_iconSizes CONSTANT)
public:
    using QObject::QObject;
    int gridUnit() const { return QFontMetrics(QApplication::font()).height(); }

private:
    int m_small = 4, m_medium = 6, m_large = 8, m_long = 200;
    QObject *m_iconSizes = new StubIconSizes(this);
};

// image://icon/<name>?color=<#rrggbb>: theme icon, optionally recoloured the
// way Kirigami recolours monochrome icons.
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        const QString name = id.section(QLatin1Char('?'), 0, 0);
        const QColor color(QUrlQuery(id.section(QLatin1Char('?'), 1)).queryItemValue(QStringLiteral("color")));
        const QSize want = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(32, 32);
        QPixmap pixmap = QIcon::fromTheme(name).pixmap(want);
        if (color.isValid() && color.alpha() > 0 && !pixmap.isNull()) {
            QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
            QPainter p(&image);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(image.rect(), color);
            p.end();
            pixmap = QPixmap::fromImage(image);
        }
        if (size) {
            *size = pixmap.size();
        }
        return pixmap;
    }
};

namespace Preview
{
namespace
{
QCommandLineOption dark(QStringLiteral("dark"), QStringLiteral("Preview: Breeze Dark colours."));
QCommandLineOption screenshot(QStringLiteral("screenshot"), QStringLiteral("Preview: save the window to FILE and quit."), QStringLiteral("file"));
QCommandLineOption delay(QStringLiteral("delay"), QStringLiteral("Preview: wait MS before the screenshot."), QStringLiteral("ms"), QStringLiteral("1500"));
QCommandLineOption startUpdate(QStringLiteral("start-update"), QStringLiteral("Preview: press Update all after loading."));
QCommandLineOption page(QStringLiteral("page"),
                        QStringLiteral("Preview: open \"output\", \"changes\", \"configs\", \"diff\" (the first settings file) or \"processes\" after MS (--page-delay)."),
                        QStringLiteral("name"));
QCommandLineOption answer(QStringLiteral("answer"), QStringLiteral("Preview: answer pacman's question with VALUE (\"cancel\" cancels)."), QStringLiteral("value"));
QCommandLineOption press(QStringLiteral("press"),
                         QStringLiteral("Preview: press KEY (Qt key name, e.g. Return, L, Escape) after MS: KEY@MS, repeatable."),
                         QStringLiteral("key@ms"));
QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Preview: window size WxH in logical pixels."), QStringLiteral("WxH"));
QCommandLineOption fontSize(QStringLiteral("font-size"), QStringLiteral("Preview: default font size in points."), QStringLiteral("pt"));
QCommandLineOption pageDelay(QStringLiteral("page-delay"), QStringLiteral("Preview: delay for --page."), QStringLiteral("ms"), QStringLiteral("500"));
}

void addOptions(QCommandLineParser &parser)
{
    parser.addOptions({dark, screenshot, delay, startUpdate, page, pageDelay, answer, press, size, fontSize});
}

void setUp(QQmlApplicationEngine &engine, const QCommandLineParser &parser)
{
    g_dark = parser.isSet(dark);
    if (parser.isSet(fontSize)) {
        QFont font = QApplication::font();
        font.setPointSizeF(parser.value(fontSize).toDouble());
        QApplication::setFont(font);
    }
    // Without a desktop platform theme Qt only searches :/icons.
    QStringList iconPaths = QIcon::themeSearchPaths();
    const QString dataDirs = qEnvironmentVariable("XDG_DATA_DIRS", QStringLiteral("/usr/local/share:/usr/share"));
    for (const QString &dir : dataDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        iconPaths.append(dir + QStringLiteral("/icons"));
    }
    QIcon::setThemeSearchPaths(iconPaths);
    QIcon::setThemeName(g_dark ? QStringLiteral("breeze-dark") : QStringLiteral("breeze"));

    // Fusion draws the stock controls from the palette; give it Breeze's.
    const Set &w = g_dark ? DarkWindow : LightWindow;
    const Set &v = g_dark ? DarkView : LightView;
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(QLatin1String(w.bg)));
    pal.setColor(QPalette::WindowText, QColor(QLatin1String(w.text)));
    pal.setColor(QPalette::Base, QColor(QLatin1String(v.bg)));
    pal.setColor(QPalette::AlternateBase, QColor(QLatin1String(v.alt)));
    pal.setColor(QPalette::Text, QColor(QLatin1String(v.text)));
    pal.setColor(QPalette::Button, QColor(QLatin1String(g_dark ? "#292c30" : "#fcfcfc")));
    pal.setColor(QPalette::ButtonText, QColor(QLatin1String(w.text)));
    pal.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
    pal.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QLatin1String(w.disabled)));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QLatin1String(w.disabled)));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(QLatin1String(w.disabled)));
    QApplication::setPalette(pal);

    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    const char *uri = "org.kde.kirigami";
    qmlRegisterUncreatableType<StubTheme>(uri, 2, 0, "Theme", QStringLiteral("attached only"));
    qmlRegisterSingletonInstance(uri, 2, 0, "Units", new StubUnits(&engine));
    qmlRegisterType(QUrl(QStringLiteral("qrc:/preview/kirigami/Icon.qml")), uri, 2, 0, "Icon");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/preview/kirigami/Heading.qml")), uri, 2, 0, "Heading");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/preview/kirigami/Separator.qml")), uri, 2, 0, "Separator");
}

void run(QQmlApplicationEngine &engine, const QCommandLineParser &parser, PanelController &panel)
{
    QObject *root = engine.rootObjects().constFirst();
    if (parser.isSet(size)) {
        const QStringList wh = parser.value(size).split(QLatin1Char('x'));
        if (wh.size() == 2) {
            root->setProperty("width", wh[0].toInt());
            root->setProperty("height", wh[1].toInt());
        }
    }
    // The window's palette (a QQuickPalette) is what the controls read.
    if (auto *palette = root->property("palette").value<QObject *>()) {
        const QPalette pal = QApplication::palette();
        const std::pair<const char *, QPalette::ColorRole> roles[] = {
            {"window", QPalette::Window}, {"windowText", QPalette::WindowText}, {"base", QPalette::Base},
            {"alternateBase", QPalette::AlternateBase}, {"text", QPalette::Text}, {"button", QPalette::Button},
            {"buttonText", QPalette::ButtonText}, {"highlight", QPalette::Highlight}, {"highlightedText", QPalette::HighlightedText},
        };
        for (const auto &[name, role] : roles) {
            palette->setProperty(name, pal.color(QPalette::Active, role));
        }
    }
    if (parser.isSet(startUpdate)) {
        QTimer::singleShot(300, &panel, &PanelController::updateAll);
    }
    if (parser.isSet(answer)) {
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
    for (const QString &spec : parser.values(press)) {
        const QString keyName = spec.section(QLatin1Char('@'), 0, 0);
        const int at = spec.section(QLatin1Char('@'), 1).toInt();
        QTimer::singleShot(at, root, [root, keyName] {
            auto *window = qobject_cast<QQuickWindow *>(root);
            const QKeySequence sequence(keyName);
            if (!window || sequence.isEmpty()) {
                qWarning("preview: cannot press %s", qPrintable(keyName));
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
                qWarning("preview: could not save %s", qPrintable(file));
            }
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
}
}

#include "preview.moc"
