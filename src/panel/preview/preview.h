#pragma once

// Development-only support for previewing the panel without KF6 (built with
// -DKYU_PREVIEW=ON): a stand-in for the Kirigami types the panel uses, with
// Breeze's colours hardcoded, and options to script screenshots.

class PanelController;
class QCommandLineParser;
class QQmlApplicationEngine;

namespace Preview
{
void addOptions(QCommandLineParser &parser);
void setUp(QQmlApplicationEngine &engine, const QCommandLineParser &parser);
void run(QQmlApplicationEngine &engine, const QCommandLineParser &parser, PanelController &panel);
}
