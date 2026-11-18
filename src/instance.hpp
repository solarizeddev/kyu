// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Manjaro fork: one window per user, and a way in for the update notifier.
//   packageinstaller-manjaro --updates   opens (or raises) the app on its Updates page
// The notifier's tray icon and notification run exactly that.

#include <QString>

class QQmlApplicationEngine;

namespace instance {

/// The page asked for on the command line ("updates"), or empty.
QString requestedPage();

/// When the app already runs for this user: asks it to come forward (and show
/// the page asked for) and returns true; this process should then exit.
bool forwardToRunning();

/// Call after the QML is loaded: shows the page asked for, and from then on
/// answers later starts.
void serve(QQmlApplicationEngine& engine);

}  // namespace instance
