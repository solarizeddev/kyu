// Safety for distributions that move in batches (Manjaro): no partial
// upgrades, another package manager waited for, only package names reach
// pacman, the update announcements offered before a full update. Kept apart
// from mainwindow.cpp so upstream's changes there merge cleanly.
#pragma once

#include <QString>

#include <functional>

class QWidget;

namespace pi::safety {

// pamac (its background refresh) or pacman holds pacman's lock: asks to
// retry until it is gone. false: cancelled.
bool wait_for_pacman_lock(QWidget* parent);

// Updates waiting in the package lists (pacman -Qu, no root); held packages
// (IgnorePkg, "[ignored]") do not count. -1: could not tell.
int pending_updates();

// Before installing or removing: the lock waited for; installing refused
// while updates wait (it would mix new and old libraries), with the update
// offered (upgrade_all). false: not now.
bool ready_to_change(QWidget* parent, bool installing, const std::function<void()>& upgrade_all);

// Before "Upgrade All": the lock waited for, the distribution's update
// announcement offered (PI_UPDATE_NOTES_URL). false: not now.
bool ready_to_upgrade_all(QWidget* parent);

// Package names as pacman allows them, separated by spaces: nothing else
// (no options) may reach the shell command and pacman.
bool valid_package_names(const QString& names);

}  // namespace pi::safety
