#pragma once

// Testing options for the panel: window size, scripted screenshots, opening a
// page, and (only with a test helper on the session bus) pressing Update all,
// answering questions and pressing keys. Used by tools/kyu-demo.sh and
// tools/kyu-screens.sh, with the real Kirigami or the preview stand-in.

class PanelController;
class QCommandLineParser;
class QQmlApplicationEngine;

namespace TestDriver
{
void addOptions(QCommandLineParser &parser);
void run(QQmlApplicationEngine &engine, const QCommandLineParser &parser, PanelController &panel, bool sessionBus);
}
