#pragma once

#include <QIcon>
#include <QPalette>

// Draws the update count onto a theme icon, at every size a tray may ask for.
// Colours come from the palette (highlight / highlighted text), which follows
// the user's colour scheme and accent; nothing is hardcoded.
QIcon badgedIcon(const QIcon &base, int count, const QPalette &palette);
